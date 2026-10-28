#include "tape_scanner/adapters/camera_simulator.hpp"
#include "tape_scanner/adapters/controller_simulator.hpp"
#include "tape_scanner/scanner/ScanDirector.hpp"
#include "tape_scanner/scanner/ScanStop.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char *expr, int line) {
  ++g_checks;
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

using tape_scanner::ApplyFinishResult;
using tape_scanner::CapturedTile;
using tape_scanner::DirectorResult;
using tape_scanner::KeepsCaptured;
using tape_scanner::MotionKind;
using tape_scanner::MotionRecord;
using tape_scanner::PlanTerminal;
using tape_scanner::RequestScanStop;
using tape_scanner::ScanDirector;
using tape_scanner::ScanHooks;
using tape_scanner::ScanJob;
using tape_scanner::ScanPhase;
using tape_scanner::ScanRunStatus;
using tape_scanner::ScanStopReason;
using tape_scanner::ScanStopSettings;
using tape_scanner::ScanStopSettingsFromText;
using tape_scanner::TerminalInput;
using tape_scanner::TerminalPlan;
using tape_scanner::TileSink;
using tape_scanner::kFilmEjectSteps;
using tape_scanner::adapters::AdapterError;
using tape_scanner::adapters::AdapterResult;
using tape_scanner::adapters::CameraFaultKind;
using tape_scanner::adapters::CameraProfile;
using tape_scanner::adapters::CameraSimulator;
using tape_scanner::adapters::CaptureRequest;
using tape_scanner::adapters::ControllerProfile;
using tape_scanner::adapters::ControllerSimulator;
using tape_scanner::adapters::ControllerMotion;
using tape_scanner::adapters::ControllerState;
using tape_scanner::adapters::Frame;
using tape_scanner::adapters::MakeCameraProfile;
using tape_scanner::adapters::MakeControllerProfile;
using tape_scanner::adapters::PixelFormat;

int Count(const std::vector<MotionRecord> &motions, MotionKind kind) {
  int count = 0;
  for (const MotionRecord &motion : motions) {
    if (motion.kind == kind) {
      ++count;
    }
  }
  return count;
}

void CheckNoTransfer(const DirectorResult &result) {
  CHECK(Count(result.motions, MotionKind::Eject) == 0);
  CHECK(Count(result.motions, MotionKind::Park) == 0);
}

CameraProfile Cam(const std::string &scanner, const std::string &serial) {
  CameraProfile profile = MakeCameraProfile(scanner, serial);
  profile.width = 4;
  profile.height = 2;
  profile.max_width = 1280;
  profile.max_height = 960;
  profile.format = PixelFormat::Mono16;
  return profile;
}

ControllerProfile Ctl(const std::string &scanner, const std::string &serial) {
  ControllerProfile profile = MakeControllerProfile(scanner, serial);
  profile.film_present = true;
  return profile;
}

class MemorySink final : public TileSink {
 public:
  bool fail_push = false;
  bool fail_finish = false;
  bool fail_commit = false;

  bool Push(const CapturedTile &tile) override {
    if (fail_push) {
      return false;
    }
    open.push_back(tile);
    return true;
  }
  bool Commit() override {
    if (fail_commit) {
      return false;
    }
    committed.insert(committed.end(), open.begin(), open.end());
    open.clear();
    return true;
  }
  bool Finish() override {
    if (fail_finish) {
      return false;
    }
    return Commit();
  }
  void DiscardOpen() override { open.clear(); }
  int Committed() const override { return static_cast<int>(committed.size()); }
  int Open() const override { return static_cast<int>(open.size()); }

  std::vector<CapturedTile> open;
  std::vector<CapturedTile> committed;
};

bool WaitUntil(const std::atomic<bool> &flag) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!flag.load()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

ScanHooks BlockOn(ScanPhase phase, std::atomic<bool> &entered, int skip = 0) {
  ScanHooks hooks;
  hooks.on_phase = [phase, &entered, skip, seen = 0](ScanPhase current, const std::atomic<bool> &cancel) mutable {
    if (current != phase) {
      return;
    }
    if (seen++ < skip) {
      return;
    }
    entered.store(true);
    while (!cancel.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  };
  return hooks;
}

void TestSettingsAndPlans() {
  CHECK(kFilmEjectSteps == 40000);
  const auto defaults = ScanStopSettingsFromText("", "");
  CHECK(defaults.hold_motors);
  CHECK(defaults.save_captured);
  const auto off = ScanStopSettingsFromText("0", "false");
  CHECK(!off.hold_motors);
  CHECK(!off.save_captured);
  const auto words = ScanStopSettingsFromText(" yes ", "ON");
  CHECK(words.hold_motors);
  CHECK(words.save_captured);
  const auto garbage = ScanStopSettingsFromText("maybe", "2");
  CHECK(garbage.hold_motors);
  CHECK(garbage.save_captured);

  const ScanStopReason reasons[] = {
      ScanStopReason::None,          ScanStopReason::OperatorStop,    ScanStopReason::EndOfMedia,
      ScanStopReason::EndOfCalibration, ScanStopReason::Finished,      ScanStopReason::MissingCalibration,
      ScanStopReason::EmptyFrame,    ScanStopReason::HdrFailed,       ScanStopReason::PushFailed,
      ScanStopReason::FinishFailed,  ScanStopReason::LinkLost,        ScanStopReason::DeviceFault,
      ScanStopReason::Exception,     ScanStopReason::NotCreated,      ScanStopReason::StopTimeout,
      ScanStopReason::PreflightFailed, ScanStopReason::FilmTimeout,   ScanStopReason::MotionTimeout,
      ScanStopReason::CaptureTimeout, ScanStopReason::WriteTimeout};
  for (ScanStopReason reason : reasons) {
    TerminalInput input;
    input.reason = reason;
    input.axes_live = true;
    input.confirmed_tiles = 2;
    const TerminalPlan plan = PlanTerminal(input);
    CHECK(!plan.eject_film);
    if (reason == ScanStopReason::OperatorStop || reason == ScanStopReason::DeviceFault ||
        reason == ScanStopReason::EmptyFrame || reason == ScanStopReason::Exception) {
      CHECK(plan.stop_axes);
      CHECK(!plan.home_axes);
    }
  }

  TerminalInput stopped;
  stopped.reason = ScanStopReason::OperatorStop;
  stopped.hold_motors = false;
  stopped.axes_live = true;
  stopped.confirmed_tiles = 1;
  const TerminalPlan operator_hold = PlanTerminal(stopped);
  CHECK(operator_hold.stop_axes);
  CHECK(!operator_hold.home_axes);
  CHECK(!operator_hold.eject_film);
  CHECK(operator_hold.status == ScanRunStatus::Partial);
  CHECK(!operator_hold.claim_success);

  TerminalInput ended;
  ended.reason = ScanStopReason::EndOfMedia;
  ended.axes_live = true;
  ended.confirmed_tiles = 2;
  ended.completeness_proven = false;
  const TerminalPlan media = PlanTerminal(ended);
  CHECK(!media.stop_axes);
  CHECK(!media.home_axes);
  CHECK(!media.eject_film);
  CHECK(media.status == ScanRunStatus::Partial);

  TerminalInput link_home;
  link_home.reason = ScanStopReason::LinkLost;
  link_home.hold_motors = false;
  link_home.controller_link_up = true;
  link_home.axes_live = true;
  link_home.confirmed_tiles = 1;
  const TerminalPlan home_on_loss = PlanTerminal(link_home);
  CHECK(!home_on_loss.stop_axes);
  CHECK(home_on_loss.home_axes);
  CHECK(!home_on_loss.eject_film);
  CHECK(home_on_loss.status == ScanRunStatus::Partial);

  TerminalInput link_down = link_home;
  link_down.controller_link_up = false;
  const TerminalPlan down = PlanTerminal(link_down);
  CHECK(!down.stop_axes);
  CHECK(!down.home_axes);

  TerminalInput link_hold = link_home;
  link_hold.hold_motors = true;
  const TerminalPlan held = PlanTerminal(link_hold);
  CHECK(held.stop_axes);
  CHECK(!held.home_axes);

  TerminalInput done;
  done.reason = ScanStopReason::Finished;
  done.completeness_proven = true;
  done.finish_ok = true;
  done.confirmed_tiles = 2;
  done.open_tiles = 0;
  TerminalPlan complete = PlanTerminal(done);
  CHECK(complete.status == ScanRunStatus::Completed);
  CHECK(complete.claim_success);
  CHECK(!complete.eject_film);
  CHECK(!complete.stop_axes);
  ApplyFinishResult(complete, done.reason, false, 2);
  CHECK(!complete.claim_success);
  CHECK(complete.status == ScanRunStatus::Partial);
  CHECK(done.reason == ScanStopReason::FinishFailed);

  TerminalInput missing;
  missing.reason = ScanStopReason::MissingCalibration;
  missing.axes_live = false;
  const TerminalPlan before_motion = PlanTerminal(missing);
  CHECK(!before_motion.stop_axes);
  CHECK(before_motion.status == ScanRunStatus::Fault);

  TerminalInput empty;
  empty.reason = ScanStopReason::EmptyFrame;
  empty.axes_live = true;
  empty.confirmed_tiles = 0;
  empty.open_tiles = 0;
  CHECK(PlanTerminal(empty).status == ScanRunStatus::Fault);
  empty.confirmed_tiles = 1;
  CHECK(PlanTerminal(empty).status == ScanRunStatus::Partial);

  TerminalInput open_only;
  open_only.reason = ScanStopReason::HdrFailed;
  open_only.axes_live = true;
  open_only.open_tiles = 1;
  open_only.save_captured = false;
  CHECK(PlanTerminal(open_only).status == ScanRunStatus::Fault);
  CHECK(!PlanTerminal(open_only).save_open);
  open_only.save_captured = true;
  CHECK(PlanTerminal(open_only).status == ScanRunStatus::Partial);
  CHECK(KeepsCaptured(open_only, true));
}

struct Rig {
  CameraSimulator camera;
  ControllerSimulator controller;
  ScanDirector director;
  MemorySink sink;

  Rig()
      : camera(Cam("scn-a", "CAM-A")),
        controller(Ctl("scn-a", "CTL-A")),
        director(camera, controller) {}
};

void TestNullAndIdleStop() {
  ScanDirector *missing = nullptr;
  const auto rejected = RequestScanStop(missing);
  CHECK(!rejected.accepted);
  CHECK(rejected.reason == ScanStopReason::NotCreated);
  CHECK(rejected.status == ScanRunStatus::Idle);

  Rig rig;
  const int motions = rig.controller.AppliedMotionCount();
  const auto idle = rig.director.RequestStop();
  CHECK(idle.accepted);
  CHECK(!idle.timed_out);
  CHECK(rig.controller.AppliedMotionCount() == motions);
  CHECK(!rig.camera.IsConnected());
  CHECK(rig.director.Result().motions.empty());
}

void TestStopPhases() {
  {
    Rig rig;
    std::atomic<bool> entered{false};
    rig.director.Start(ScanJob{}, rig.sink, BlockOn(ScanPhase::Connecting, entered));
    CHECK(WaitUntil(entered));
    const auto stop = rig.director.RequestStop();
    CHECK(stop.accepted);
    CHECK(!stop.timed_out);
    const DirectorResult result = rig.director.Result();
    CHECK(result.reason == ScanStopReason::OperatorStop);
    CHECK(result.status == ScanRunStatus::Partial);
    CHECK(!result.claim_success);
    CHECK(!rig.camera.IsConnected());
    CHECK(!rig.controller.IsConnected());
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::ReferenceHome) == 0);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 0);
  }
  {
    Rig rig;
    std::atomic<bool> entered{false};
    ScanHooks hooks = BlockOn(ScanPhase::WaitingForFilm, entered);
    hooks.film_present = [] { return false; };
    rig.director.Start(ScanJob{}, rig.sink, std::move(hooks));
    CHECK(WaitUntil(entered));
    CHECK(rig.director.RequestStop().accepted);
    const DirectorResult result = rig.director.Result();
    CHECK(result.reason == ScanStopReason::OperatorStop);
    CHECK(result.phase == ScanPhase::WaitingForFilm);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::ReferenceHome) == 0);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
    ControllerState state;
    CHECK(rig.controller.GetState(state));
    CHECK(state.motion == ControllerMotion::Stopped);
  }
  {
    Rig rig;
    std::atomic<bool> entered{false};
    rig.director.Start(ScanJob{}, rig.sink, BlockOn(ScanPhase::Moving, entered));
    CHECK(WaitUntil(entered));
    const int before = rig.controller.AppliedMotionCount();
    CHECK(rig.director.RequestStop().accepted);
    const DirectorResult result = rig.director.Result();
    CHECK(result.reason == ScanStopReason::OperatorStop);
    CHECK(result.message == "stop during motion");
    CHECK(Count(result.motions, MotionKind::ReferenceHome) == 1);
    CHECK(Count(result.motions, MotionKind::MoveCarriage) == 1);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
    CheckNoTransfer(result);
    CHECK(rig.controller.AppliedMotionCount() == before);
    ControllerState state;
    CHECK(rig.controller.GetState(state));
    CHECK(state.motion == ControllerMotion::Stopped);
    CHECK(state.coordinates.carriage.accepted.available);
    CHECK(state.coordinates.carriage.accepted.value == 10);
    const auto ejected = rig.director.EjectFilm();
    CHECK(ejected.code == AdapterError::Rejected);
    CHECK(rig.controller.AppliedMotionCount() == before);
    CHECK(Count(rig.director.Result().motions, MotionKind::Eject) == 0);
  }
  {
    Rig rig;
    std::atomic<bool> entered{false};
    rig.director.Start(ScanJob{}, rig.sink, BlockOn(ScanPhase::WaitingFrame, entered));
    CHECK(WaitUntil(entered));
    CHECK(rig.director.RequestStop().accepted);
    const DirectorResult result = rig.director.Result();
    CHECK(result.reason == ScanStopReason::OperatorStop);
    CHECK(result.phase == ScanPhase::WaitingFrame);
    CHECK(rig.camera.CompletedCaptures() == 0);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
  }
  {
    Rig rig;
    std::atomic<bool> entered{false};
    ScanJob job;
    job.expected_tiles = 2;
    rig.director.Start(job, rig.sink, BlockOn(ScanPhase::Writing, entered, 1));
    CHECK(WaitUntil(entered));
    CHECK(rig.director.RequestStop().accepted);
    const DirectorResult result = rig.director.Result();
    CHECK(result.reason == ScanStopReason::OperatorStop);
    CHECK(result.phase == ScanPhase::Writing);
    CHECK(result.status == ScanRunStatus::Partial);
    CHECK(result.confirmed_tiles == 2);
    CHECK(rig.sink.Committed() == 2);
    CheckNoTransfer(result);
  }
  {
    Rig rig;
    ScanStopSettings settings;
    settings.save_captured = false;
    rig.director.SetSettings(settings);
    std::atomic<bool> entered{false};
    ScanJob job;
    job.expected_tiles = 2;
    rig.director.Start(job, rig.sink, BlockOn(ScanPhase::Writing, entered, 1));
    CHECK(WaitUntil(entered));
    CHECK(rig.director.RequestStop().accepted);
    const DirectorResult result = rig.director.Result();
    CHECK(result.confirmed_tiles == 1);
    CHECK(rig.sink.Committed() == 1);
    CHECK(rig.sink.Open() == 0);
    CheckNoTransfer(result);
  }
}

void TestFaults() {
  {
    Rig rig;
    ScanJob job;
    job.calibration_present = false;
    const DirectorResult result = rig.director.Run(job, rig.sink);
    CHECK(result.reason == ScanStopReason::MissingCalibration);
    CHECK(result.status == ScanRunStatus::Fault);
    CHECK(!result.claim_success);
    CHECK(!rig.camera.IsConnected());
    CHECK(result.motions.empty());
    CHECK(rig.controller.AppliedMotionCount() == 0);
  }
  {
    Rig rig;
    ScanJob job;
    job.camera_serial = "NOPE";
    const DirectorResult result = rig.director.Run(job, rig.sink);
    CHECK(result.reason == ScanStopReason::PreflightFailed);
    CHECK(result.status == ScanRunStatus::Fault);
    CHECK(result.motions.empty());
  }
  {
    Rig rig;
    ScanHooks hooks;
    hooks.capture = [&](const CaptureRequest &request, Frame &frame) {
      const AdapterResult shot = rig.camera.Capture(request, frame);
      frame.buffer = {};
      frame.width = 0;
      frame.height = 0;
      return shot;
    };
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink, hooks);
    CHECK(result.reason == ScanStopReason::EmptyFrame);
    CHECK(result.status == ScanRunStatus::Fault);
    CHECK(result.confirmed_tiles == 0);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
  }
  {
    Rig rig;
    ScanJob job;
    job.exposures_per_tile = 2;
    int shots = 0;
    ScanHooks hooks;
    hooks.capture = [&](const CaptureRequest &request, Frame &frame) {
      ++shots;
      if (shots == 2) {
        return AdapterResult::Fail(AdapterError::Fault, "hdr merge failed");
      }
      return rig.camera.Capture(request, frame);
    };
    const DirectorResult result = rig.director.Run(job, rig.sink, hooks);
    CHECK(result.reason == ScanStopReason::HdrFailed);
    CHECK(result.status == ScanRunStatus::Fault);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
  }
  {
    Rig rig;
    rig.sink.fail_push = true;
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink);
    CHECK(result.reason == ScanStopReason::PushFailed);
    CHECK(result.status == ScanRunStatus::Fault);
    CHECK(!result.claim_success);
    CheckNoTransfer(result);
  }
  {
    Rig rig;
    rig.sink.fail_finish = true;
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink);
    CHECK(result.reason == ScanStopReason::FinishFailed);
    CHECK(result.status == ScanRunStatus::Partial);
    CHECK(!result.claim_success);
    CHECK(result.confirmed_tiles == 1);
    CheckNoTransfer(result);
  }
  {
    Rig rig;
    rig.camera.QueueFault(CameraFaultKind::Dropped);
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink);
    CHECK(result.reason == ScanStopReason::DeviceFault);
    CHECK(result.status != ScanRunStatus::Completed);
    CheckNoTransfer(result);
    const int motions = rig.controller.AppliedMotionCount();
    const auto again = rig.director.RequestStop();
    CHECK(again.accepted);
    CHECK(!again.timed_out);
    CHECK(rig.controller.AppliedMotionCount() == motions);
    CHECK(Count(rig.director.Result().motions, MotionKind::Park) == 0);
    CHECK(Count(rig.director.Result().motions, MotionKind::Eject) == 0);
  }
  {
    Rig rig;
    ScanHooks hooks;
    hooks.on_phase = [](ScanPhase phase, const std::atomic<bool> &) {
      if (phase == ScanPhase::WaitingFrame) {
        throw std::runtime_error("worker failed");
      }
    };
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink, hooks);
    CHECK(result.reason == ScanStopReason::Exception);
    CHECK(result.status == ScanRunStatus::Fault);
    CHECK(result.message == "worker failed");
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
  }
}

void TestLinkLossAndMedia() {
  {
    Rig rig;
    rig.camera.QueueFault(CameraFaultKind::LinkLost);
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink);
    CHECK(result.reason == ScanStopReason::LinkLost);
    CHECK(result.status == ScanRunStatus::Fault);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 1);
    ControllerState state;
    CHECK(rig.controller.GetState(state));
    CHECK(state.motion == ControllerMotion::Stopped);
    CHECK(state.coordinates.carriage.accepted.value == 10);
  }
  {
    Rig rig;
    ScanStopSettings settings;
    settings.hold_motors = false;
    rig.director.SetSettings(settings);
    rig.camera.QueueFault(CameraFaultKind::LinkLost);
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink);
    CHECK(result.reason == ScanStopReason::LinkLost);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 0);
    CHECK(Count(result.motions, MotionKind::Park) == 1);
    CHECK(Count(result.motions, MotionKind::Eject) == 0);
    ControllerState state;
    CHECK(rig.controller.GetState(state));
    CHECK(state.coordinates.carriage.accepted.value == 0);
  }
  {
    Rig rig;
    ScanJob job;
    job.expected_tiles = 2;
    ScanHooks hooks;
    hooks.on_phase = [&](ScanPhase phase, const std::atomic<bool> &) {
      if (phase == ScanPhase::Moving) {
        rig.controller.SetLinkUp(false);
      }
    };
    const DirectorResult result = rig.director.Run(job, rig.sink, hooks);
    CHECK(result.reason == ScanStopReason::LinkLost);
    CHECK(result.status == ScanRunStatus::Partial);
    CHECK(result.confirmed_tiles == 1);
    CHECK(rig.sink.Committed() == 1);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 0);
    CHECK(Count(result.motions, MotionKind::ReferenceHome) == 1);
  }
  {
    Rig rig;
    ScanJob job;
    job.expected_tiles = 3;
    ScanHooks hooks;
    hooks.film_present = [&] { return rig.sink.Committed() < 1; };
    const DirectorResult result = rig.director.Run(job, rig.sink, hooks);
    CHECK(result.reason == ScanStopReason::EndOfMedia);
    CHECK(result.status == ScanRunStatus::Partial);
    CHECK(!result.claim_success);
    CHECK(result.confirmed_tiles == 1);
    CheckNoTransfer(result);
    CHECK(Count(result.motions, MotionKind::StopAxes) == 0);
    ControllerState state;
    CHECK(rig.controller.GetState(state));
    CHECK(state.motion == ControllerMotion::Idle);
    CHECK(state.coordinates.carriage.accepted.value == 10);
  }
  {
    Rig rig;
    const DirectorResult result = rig.director.Run(ScanJob{}, rig.sink);
    CHECK(result.reason == ScanStopReason::Finished);
    CHECK(result.status == ScanRunStatus::Completed);
    CHECK(result.claim_success);
    CHECK(result.confirmed_tiles == 1);
    CheckNoTransfer(result);
    ControllerState before;
    CHECK(rig.controller.GetState(before));
    CHECK(before.motion == ControllerMotion::Idle);
    const auto ejected = rig.director.EjectFilm();
    CHECK(ejected);
    ControllerState after;
    CHECK(rig.controller.GetState(after));
    CHECK(after.coordinates.film.accepted.value == static_cast<double>(kFilmEjectSteps));
    CHECK(Count(rig.director.Result().motions, MotionKind::Eject) == 1);
    CHECK(rig.director.Park());
    CHECK(Count(rig.director.Result().motions, MotionKind::Park) == 1);
  }
}

void TestStopTimeoutAndIsolation() {
  CameraSimulator camera_b(Cam("scn-b", "CAM-B"));
  ControllerSimulator controller_b(Ctl("scn-b", "CTL-B"));
  CHECK(controller_b.Connect({"CTL-B"}));
  CHECK(controller_b.Home());
  const int motions_b = controller_b.AppliedMotionCount();

  Rig rig;
  ScanStopSettings settings;
  settings.stop_deadline = std::chrono::milliseconds(40);
  rig.director.SetSettings(settings);
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
  struct ReleaseOnExit {
    std::atomic<bool> &flag;
    ~ReleaseOnExit() { flag.store(true); }
  } release_on_exit{release};
  ScanHooks hooks;
  hooks.on_phase = [&](ScanPhase phase, const std::atomic<bool> &) {
    if (phase != ScanPhase::Moving) {
      return;
    }
    entered.store(true);
    while (!release.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  };
  rig.director.Start(ScanJob{}, rig.sink, std::move(hooks));
  CHECK(WaitUntil(entered));
  const auto started = std::chrono::steady_clock::now();
  const auto stop = rig.director.RequestStop();
  const auto elapsed = std::chrono::steady_clock::now() - started;
  CHECK(stop.accepted);
  CHECK(stop.timed_out);
  CHECK(stop.reason == ScanStopReason::StopTimeout);
  CHECK(elapsed < std::chrono::milliseconds(500));
  CHECK(elapsed >= std::chrono::milliseconds(30));
  release.store(true);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (rig.director.Phase() != ScanPhase::Finished && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(rig.director.Phase() == ScanPhase::Finished);
  const DirectorResult result = rig.director.Result();
  CHECK(result.reason == ScanStopReason::OperatorStop);
  CheckNoTransfer(result);
  CHECK(Count(result.motions, MotionKind::StopAxes) == 1);

  ControllerState state_b;
  CHECK(controller_b.GetState(state_b));
  CHECK(controller_b.AppliedMotionCount() == motions_b);
  CHECK(state_b.motion == ControllerMotion::Idle);
  CHECK(state_b.coordinates.carriage.accepted.value == 0);
  (void)camera_b;
}

}  // namespace

int main() {
  TestSettingsAndPlans();
  TestNullAndIdleStop();
  TestStopPhases();
  TestFaults();
  TestLinkLossAndMedia();
  TestStopTimeoutAndIsolation();

  if (g_failures != 0) {
    std::cerr << g_failures << " scan-stop checks failed\n";
    return 1;
  }
  std::cout << g_checks << " scan-stop checks passed\n";
  return 0;
}

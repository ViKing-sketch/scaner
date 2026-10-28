#include "tape_scanner/scanner/ScanDirector.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

namespace tape_scanner {
namespace {

using clock = std::chrono::steady_clock;

adapters::Field<double> Commanded(double value) {
  return adapters::MakeField(value, adapters::FieldOrigin::Commanded);
}

}  // namespace

ScanDirector::ScanDirector(adapters::ICamera &camera, adapters::IController &controller,
                           ScanStopSettings settings)
    : m_camera(camera), m_controller(controller), m_settings(std::move(settings)) {}

ScanDirector::~ScanDirector() {
  m_cancel.store(true);
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

void ScanDirector::SetSettings(ScanStopSettings settings) {
  std::lock_guard lock(m_mu);
  if (m_running) {
    return;
  }
  m_settings = std::move(settings);
}

ScanPhase ScanDirector::Phase() const { return m_phase.load(); }

DirectorResult ScanDirector::Result() const {
  std::lock_guard lock(m_mu);
  return m_result;
}

void ScanDirector::Record(MotionKind kind, double position) {
  MotionRecord record;
  record.kind = kind;
  record.position = position;
  record.phase = m_phase.load();
  std::lock_guard lock(m_mu);
  m_result.motions.push_back(record);
}

bool ScanDirector::Enter(ScanPhase phase) {
  m_phase.store(phase);
  if (m_hooks.on_phase) {
    m_hooks.on_phase(phase, m_cancel);
  }
  return !m_cancel.load();
}

bool ScanDirector::FilmPresent() {
  if (m_hooks.film_present) {
    return m_hooks.film_present();
  }
  adapters::ControllerState state;
  const auto result = m_controller.GetState(state);
  if (!result) {
    if (result.code == adapters::AdapterError::LinkLost) {
      m_input.controller_link_up = false;
      m_input.reason = ScanStopReason::LinkLost;
      m_input.message = result.message;
    }
    return false;
  }
  return state.film_present.available && state.film_present.value;
}

void ScanDirector::RememberCounts() {
  if (m_sink == nullptr) {
    return;
  }
  m_input.confirmed_tiles = m_sink->Committed();
  m_input.open_tiles = m_sink->Open();
}

void ScanDirector::Light(bool on) {
  if (!m_controller.IsConnected()) {
    return;
  }
  const auto result = m_controller.SetLight(on);
  if (!result) {
    return;
  }
  Record(on ? MotionKind::LightOn : MotionKind::LightOff, on ? 1 : 0);
}

adapters::AdapterResult ScanDirector::CaptureOne(std::uint64_t tile_id, std::uint64_t exposure_id,
                                                 adapters::Frame &frame) {
  adapters::CaptureRequest request;
  request.scanner_id = m_job.scanner_id;
  request.session_id = m_job.session_id;
  request.tile_id = tile_id;
  request.exposure_id = exposure_id;
  adapters::ControllerState state;
  if (m_controller.GetState(state)) {
    request.coordinates = state.coordinates;
    request.coordinates.carriage.commanded = Commanded(state.coordinates.carriage.accepted.available
                                                           ? state.coordinates.carriage.accepted.value
                                                           : 0);
  }
  if (m_hooks.capture) {
    return m_hooks.capture(request, frame);
  }
  return m_camera.Capture(request, frame);
}

void ScanDirector::ClassifyCapture(adapters::AdapterError code, const std::string &message) {
  m_input.message = message;
  if (code == adapters::AdapterError::LinkLost) {
    m_input.reason = ScanStopReason::LinkLost;
    if (!m_controller.IsConnected()) {
      m_input.controller_link_up = false;
    }
    return;
  }
  if (code == adapters::AdapterError::Timeout) {
    m_input.reason = ScanStopReason::CaptureTimeout;
    return;
  }
  if (m_job.exposures_per_tile > 1) {
    m_input.reason = ScanStopReason::HdrFailed;
    return;
  }
  m_input.reason = ScanStopReason::DeviceFault;
}

void ScanDirector::RunJob() {
  if (m_cancel.load()) {
    m_input.reason = ScanStopReason::OperatorStop;
    m_input.message = "stop before the run";
    return;
  }
  if (m_job.require_calibration && !m_job.calibration_present) {
    m_input.reason = ScanStopReason::MissingCalibration;
    m_input.message = "calibration is missing";
    return;
  }
  if (m_job.expected_tiles < 1 || m_job.exposures_per_tile < 1) {
    m_input.reason = ScanStopReason::PreflightFailed;
    m_input.message = "scan job has no tiles";
    return;
  }

  if (!Enter(ScanPhase::Connecting)) {
    m_input.reason = ScanStopReason::OperatorStop;
    m_input.message = "stop before connection";
    return;
  }

  const auto camera = m_camera.Connect({m_job.camera_serial, ""});
  if (!camera) {
    m_input.reason = camera.code == adapters::AdapterError::LinkLost ? ScanStopReason::LinkLost
                                                                     : ScanStopReason::PreflightFailed;
    m_input.message = camera.message;
    return;
  }
  const auto controller = m_controller.Connect({m_job.controller_serial});
  if (!controller) {
    m_input.reason = controller.code == adapters::AdapterError::LinkLost
                         ? ScanStopReason::LinkLost
                         : ScanStopReason::PreflightFailed;
    m_input.message = controller.message;
    m_input.controller_link_up = controller.code != adapters::AdapterError::LinkLost;
    return;
  }
  const auto session = m_camera.StartSession(m_job.scanner_id, m_job.session_id);
  if (!session) {
    m_input.reason = ScanStopReason::PreflightFailed;
    m_input.message = session.message;
    return;
  }

  if (!Enter(ScanPhase::WaitingForFilm)) {
    m_input.reason = ScanStopReason::OperatorStop;
    m_input.message = "stop while waiting for film";
    return;
  }
  const auto film_deadline = clock::now() + m_settings.film_wait;
  while (!FilmPresent()) {
    if (m_input.reason == ScanStopReason::LinkLost) {
      return;
    }
    if (m_cancel.load()) {
      m_input.reason = ScanStopReason::OperatorStop;
      m_input.message = "stop while waiting for film";
      return;
    }
    if (clock::now() >= film_deadline) {
      m_input.reason = ScanStopReason::FilmTimeout;
      m_input.message = "film was not detected before the deadline";
      return;
    }
    if (!m_hooks.film_present) {
      // Датчик симулятора не меняется. Без подмены нет смысла ждать 30 минут.
      m_input.reason = ScanStopReason::FilmTimeout;
      m_input.message = "film is not present";
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  if (!Enter(ScanPhase::Referencing)) {
    m_input.reason = ScanStopReason::OperatorStop;
    m_input.message = "stop before reference";
    return;
  }
  const auto home_started = clock::now();
  const auto home = m_controller.Home();
  if (clock::now() - home_started > m_settings.motion_deadline && !m_cancel.load()) {
    m_input.reason = ScanStopReason::MotionTimeout;
    m_input.message = "reference exceeded the deadline";
    m_input.axes_live = true;
    return;
  }
  if (!home) {
    m_input.axes_live = home.code != adapters::AdapterError::NotConnected;
    ClassifyCapture(home.code, home.message);
    if (home.code != adapters::AdapterError::LinkLost && home.code != adapters::AdapterError::Timeout) {
      m_input.reason = ScanStopReason::DeviceFault;
      m_input.message = home.message;
    }
    return;
  }
  m_input.axes_live = true;
  Record(MotionKind::ReferenceHome, 0);
  Light(true);

  int captured = 0;
  while (captured < m_job.expected_tiles) {
    if (m_cancel.load()) {
      m_input.reason = ScanStopReason::OperatorStop;
      m_input.message = "stop";
      return;
    }
    if (!FilmPresent()) {
      if (m_input.reason == ScanStopReason::LinkLost) {
        return;
      }
      m_input.reason = ScanStopReason::EndOfMedia;
      m_input.message = "carrier ended";
      m_input.completeness_proven = false;
      return;
    }

    const double carriage = m_job.carriage_origin + captured * m_job.carriage_step;
    const double film = m_job.film_origin + captured * m_job.film_step;
    m_phase.store(ScanPhase::Moving);
    const auto move_started = clock::now();
    const auto moved = m_controller.Move(
        {std::string("mv") + std::to_string(++m_command_serial), adapters::Axis::Carriage, carriage});
    if (!moved) {
      m_input.axes_live = true;
      if (moved.code == adapters::AdapterError::LinkLost) {
        m_input.reason = ScanStopReason::LinkLost;
        m_input.controller_link_up = m_controller.IsConnected();
      } else if (moved.code == adapters::AdapterError::Timeout) {
        // Ответ потерян. Положение не повторяем вторым Move.
        m_input.reason = ScanStopReason::MotionTimeout;
      } else {
        m_input.reason = ScanStopReason::DeviceFault;
      }
      m_input.message = moved.message;
      return;
    }
    Record(MotionKind::MoveCarriage, carriage);
    if (m_job.film_step != 0.0) {
      const auto film_move = m_controller.Move(
          {std::string("fm") + std::to_string(++m_command_serial), adapters::Axis::Film, film});
      if (!film_move) {
        m_input.reason = film_move.code == adapters::AdapterError::LinkLost ? ScanStopReason::LinkLost
                                                                            : ScanStopReason::DeviceFault;
        m_input.message = film_move.message;
        m_input.controller_link_up = m_controller.IsConnected();
        return;
      }
      Record(MotionKind::MoveFilm, film);
    }
    if (clock::now() - move_started > m_settings.motion_deadline && !m_cancel.load()) {
      m_input.reason = ScanStopReason::MotionTimeout;
      m_input.message = "move exceeded the deadline";
      return;
    }
    if (m_hooks.on_phase) {
      m_hooks.on_phase(ScanPhase::Moving, m_cancel);
    }
    if (m_cancel.load()) {
      m_input.reason = ScanStopReason::OperatorStop;
      m_input.message = "stop during motion";
      return;
    }

    if (!Enter(ScanPhase::WaitingFrame)) {
      m_input.reason = ScanStopReason::OperatorStop;
      m_input.message = "stop while waiting for a frame";
      return;
    }
    const auto capture_started = clock::now();
    for (int exposure = 0; exposure < m_job.exposures_per_tile; ++exposure) {
      adapters::Frame frame;
      const auto shot = CaptureOne(static_cast<std::uint64_t>(captured + 1),
                                   static_cast<std::uint64_t>(exposure + 1), frame);
      if (!shot) {
        ClassifyCapture(shot.code, shot.message);
        return;
      }
      if (frame.buffer.empty() || frame.width <= 0 || frame.height <= 0) {
        m_input.reason = ScanStopReason::EmptyFrame;
        m_input.message = "empty frame";
        return;
      }
    }
    if (clock::now() - capture_started > m_settings.capture_deadline && !m_cancel.load()) {
      m_input.reason = ScanStopReason::CaptureTimeout;
      m_input.message = "capture exceeded the deadline";
      return;
    }
    if (m_cancel.load()) {
      m_input.reason = ScanStopReason::OperatorStop;
      m_input.message = "stop while waiting for a frame";
      return;
    }

    m_phase.store(ScanPhase::Writing);
    CapturedTile tile;
    tile.tile_id = static_cast<std::uint64_t>(captured + 1);
    tile.carriage = carriage;
    tile.film = film;
    const auto write_started = clock::now();
    if (m_sink == nullptr || !m_sink->Push(tile)) {
      RememberCounts();
      m_input.reason = ScanStopReason::PushFailed;
      m_input.message = "tile was not accepted";
      return;
    }
    RememberCounts();
    if (m_hooks.on_phase) {
      m_hooks.on_phase(ScanPhase::Writing, m_cancel);
    }
    if (clock::now() - write_started > m_settings.write_deadline && !m_cancel.load()) {
      m_input.reason = ScanStopReason::WriteTimeout;
      m_input.message = "write exceeded the deadline";
      return;
    }
    if (m_cancel.load()) {
      m_input.reason = ScanStopReason::OperatorStop;
      m_input.message = "stop during write";
      return;
    }
    if (!m_sink->Commit()) {
      RememberCounts();
      m_input.reason = ScanStopReason::PushFailed;
      m_input.message = "tile was not committed";
      return;
    }
    RememberCounts();
    ++captured;
  }

  m_input.reason = ScanStopReason::Finished;
  m_input.message = "expected tiles captured";
  m_input.completeness_proven = captured == m_job.expected_tiles && m_input.open_tiles == 0 &&
                                m_input.confirmed_tiles == m_job.expected_tiles;
}

void ScanDirector::ApplyTerminal() {
  const ScanPhase interrupted = m_phase.load();
  m_phase.store(ScanPhase::Stopping);
  RememberCounts();
  if (m_input.reason == ScanStopReason::None) {
    m_input.reason = ScanStopReason::Exception;
    m_input.message = "scan ended without a reason";
  }
  m_input.hold_motors = m_settings.hold_motors;
  m_input.save_captured = m_settings.save_captured;

  auto plan = PlanTerminal(m_input);
  if (plan.stop_axes && m_controller.IsConnected()) {
    const auto stopped = m_controller.StopScan();
    Record(MotionKind::StopAxes, 0);
    if (!stopped && stopped.code == adapters::AdapterError::LinkLost) {
      m_input.controller_link_up = false;
    }
  } else if (plan.home_axes && m_controller.IsConnected()) {
    const auto parked = m_controller.Home();
    if (parked) {
      Record(MotionKind::Park, 0);
    }
  }

  if (plan.light_off) {
    Light(false);
  }

  bool finish_ok = true;
  if (m_sink != nullptr) {
    if (plan.save_open) {
      finish_ok = m_sink->Finish();
    } else {
      m_sink->DiscardOpen();
    }
  }
  int committed = m_sink != nullptr ? m_sink->Committed() : 0;
  ApplyFinishResult(plan, m_input.reason, finish_ok, committed);
  if (!finish_ok && m_input.message.empty()) {
    m_input.message = "finish failed";
  }

  if (m_camera.IsConnected()) {
    const auto closed = m_camera.StopSession();
    (void)closed;
  }

  DirectorResult result;
  result.status = plan.status;
  result.reason = m_input.reason;
  result.message = m_input.message;
  result.confirmed_tiles = committed;
  result.open_tiles = m_sink != nullptr ? m_sink->Open() : 0;
  result.claim_success = plan.claim_success;
  result.phase = interrupted;
  {
    std::lock_guard lock(m_mu);
    result.motions = std::move(m_result.motions);
    m_result = std::move(result);
  }
  m_phase.store(ScanPhase::Finished);
}

void ScanDirector::Execute() {
  m_input = {};
  m_input.hold_motors = m_settings.hold_motors;
  m_input.save_captured = m_settings.save_captured;
  m_input.controller_link_up = true;
  m_command_serial = 0;
  m_phase.store(ScanPhase::Idle);
  {
    std::lock_guard lock(m_mu);
    m_result = {};
  }
  try {
    RunJob();
  } catch (const std::exception &ex) {
    if (m_input.reason == ScanStopReason::None) {
      m_input.reason = ScanStopReason::Exception;
      m_input.message = ex.what();
    }
  } catch (...) {
    if (m_input.reason == ScanStopReason::None) {
      m_input.reason = ScanStopReason::Exception;
      m_input.message = "unknown exception";
    }
  }
  try {
    ApplyTerminal();
  } catch (const std::exception &ex) {
    std::lock_guard lock(m_mu);
    m_result.status = ScanRunStatus::Fault;
    m_result.reason = ScanStopReason::Exception;
    m_result.message = ex.what();
    m_result.claim_success = false;
  } catch (...) {
    std::lock_guard lock(m_mu);
    m_result.status = ScanRunStatus::Fault;
    m_result.reason = ScanStopReason::Exception;
    m_result.message = "unknown exception";
    m_result.claim_success = false;
  }
}

void ScanDirector::ThreadMain() {
  m_worker_id = std::this_thread::get_id();
  Execute();
  std::lock_guard lock(m_mu);
  m_done = true;
  m_running = false;
  m_cv.notify_all();
}

DirectorResult ScanDirector::Run(const ScanJob &job, TileSink &sink, ScanHooks hooks) {
  {
    std::lock_guard lock(m_mu);
    if (m_running) {
      throw std::logic_error("scan is already running");
    }
    m_running = true;
    m_done = false;
    m_job = job;
    m_sink = &sink;
    m_hooks = std::move(hooks);
    m_cancel.store(false);
  }
  ThreadMain();
  return Result();
}

void ScanDirector::Start(const ScanJob &job, TileSink &sink, ScanHooks hooks) {
  std::lock_guard lock(m_mu);
  if (m_running || m_thread.joinable()) {
    throw std::logic_error("scan is already running");
  }
  m_running = true;
  m_done = false;
  m_job = job;
  m_sink = &sink;
  m_hooks = std::move(hooks);
  m_cancel.store(false);
  m_thread = std::thread([this] { ThreadMain(); });
}

ScanStopResult ScanDirector::RequestStop() {
  m_cancel.store(true);
  ScanStopResult result;
  result.accepted = true;
  if (m_worker_id == std::this_thread::get_id()) {
    result.reason = ScanStopReason::OperatorStop;
    result.status = ScanRunStatus::Partial;
    return result;
  }
  std::unique_lock lock(m_mu);
  if (!m_running && !m_thread.joinable()) {
    result.reason = m_done ? m_result.reason : ScanStopReason::OperatorStop;
    result.status = m_done ? m_result.status : ScanRunStatus::Idle;
    result.run = m_result;
    return result;
  }
  const bool finished =
      m_cv.wait_for(lock, m_settings.stop_deadline, [this] { return m_done; });
  if (!finished) {
    result.timed_out = true;
    result.reason = ScanStopReason::StopTimeout;
    result.status = ScanRunStatus::Fault;
    result.run = m_result;
    return result;
  }
  lock.unlock();
  if (m_thread.joinable()) {
    m_thread.join();
  }
  lock.lock();
  result.reason = m_result.reason;
  result.status = m_result.status;
  result.run = m_result;
  return result;
}

adapters::AdapterResult ScanDirector::EjectFilm() {
  {
    std::lock_guard lock(m_mu);
    if (m_running) {
      return adapters::AdapterResult::Fail(adapters::AdapterError::Rejected, "scan is running");
    }
    if (m_result.status != ScanRunStatus::Completed) {
      return adapters::AdapterResult::Fail(adapters::AdapterError::Rejected,
                                           "eject is only available after a normal completion");
    }
  }
  adapters::ControllerState state;
  if (!m_controller.GetState(state) || !state.coordinates.film.accepted.available) {
    return adapters::AdapterResult::Fail(adapters::AdapterError::Rejected, "film position is unknown");
  }
  const double target = state.coordinates.film.accepted.value + static_cast<double>(kFilmEjectSteps);
  const auto moved = m_controller.Move(
      {std::string("eject") + std::to_string(++m_command_serial), adapters::Axis::Film, target});
  if (moved) {
    Record(MotionKind::Eject, target);
  }
  return moved;
}

adapters::AdapterResult ScanDirector::Park() {
  {
    std::lock_guard lock(m_mu);
    if (m_running) {
      return adapters::AdapterResult::Fail(adapters::AdapterError::Rejected, "scan is running");
    }
    if (m_result.status != ScanRunStatus::Completed) {
      return adapters::AdapterResult::Fail(adapters::AdapterError::Rejected,
                                           "park is only available after a normal completion");
    }
  }
  m_phase.store(ScanPhase::Finished);
  const auto home = m_controller.Home();
  if (home) {
    Record(MotionKind::Park, 0);
  }
  return home;
}

}  // namespace tape_scanner

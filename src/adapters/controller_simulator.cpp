#include "tape_scanner/adapters/controller_simulator.hpp"

#include <stdexcept>
#include <utility>

namespace tape_scanner::adapters {
namespace {

constexpr std::size_t kEventLimit = 64;

[[noreturn]] void InvalidProfile(const std::string& message) {
  throw std::invalid_argument(message);
}

void ValidateProfile(const ControllerProfile& profile) {
  if (!IsDeviceId(profile.scanner_id) || !IsDeviceId(profile.serial) || profile.model.empty()) {
    InvalidProfile("controller profile needs scanner_id, serial and model");
  }
  if (!(profile.step > 0.0) || !(profile.carriage_min <= profile.carriage_max) ||
      !(profile.film_min <= profile.film_max)) {
    InvalidProfile("controller profile limits are invalid");
  }
  if (profile.protocol_version.empty()) {
    InvalidProfile("controller protocol version is empty");
  }
}

Field<bool> KnownFlag(bool value) { return MakeField(value, FieldOrigin::Device); }

}  // namespace

ControllerSimulator::ControllerSimulator(ControllerProfile profile)
    : m_profile(std::move(profile)) {
  ValidateProfile(m_profile);
  m_carriage.unit = "step";
  m_film.unit = "step";
}

int ControllerSimulator::InterfaceVersion() const { return kAdapterInterfaceVersion; }

AdapterResult ControllerSimulator::Connect(const ControllerEndpoint& endpoint) {
  std::lock_guard lock(m_mutex);
  if (!IsDeviceId(endpoint.serial)) {
    PushEventLocked("connect", AdapterError::ParameterRejected);
    return AdapterResult::Fail(AdapterError::ParameterRejected, "controller serial is required");
  }
  if (endpoint.serial != m_profile.serial) {
    PushEventLocked("connect", AdapterError::WrongDevice);
    return AdapterResult::Fail(AdapterError::WrongDevice, "controller serial does not match");
  }
  if (!m_link_up) {
    PushEventLocked("connect", AdapterError::LinkLost);
    return AdapterResult::Fail(AdapterError::LinkLost, "controller link is down");
  }
  if (m_occupied || m_bound) {
    PushEventLocked("connect", AdapterError::Busy);
    return AdapterResult::Fail(AdapterError::Busy, "controller is occupied");
  }

  m_bound = true;
  m_motion = ControllerMotion::Idle;
  m_armed = false;
  m_referenced = false;
  ClearJobLocked();
  m_carriage = {};
  m_carriage.unit = "step";
  m_film = {};
  m_film.unit = "step";
  m_last_command_id.clear();
  m_light = KnownFlag(false);
  m_film_present =
      m_profile.report_film_sensor ? KnownFlag(m_profile.film_present) : UnavailableField<bool>();
  m_carriage_positive = KnownFlag(false);
  m_carriage_negative = KnownFlag(false);
  m_film_positive = KnownFlag(false);
  m_film_negative = KnownFlag(false);
  m_speed = UnavailableReal();
  m_fault_code.clear();
  m_lock_source.clear();
  PushEventLocked("connect", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::Disconnect() {
  std::lock_guard lock(m_mutex);
  if (!m_bound) {
    return AdapterResult::Fail(AdapterError::NotConnected, "controller is not connected");
  }
  m_bound = false;
  m_armed = false;
  m_referenced = false;
  m_motion = ControllerMotion::Disconnected;
  ClearJobLocked();
  m_last_command_id.clear();
  m_fault_code.clear();
  m_lock_source.clear();
  m_carriage = {};
  m_carriage.unit = "step";
  m_film = {};
  m_film.unit = "step";
  m_light = UnavailableField<bool>();
  m_film_present = UnavailableField<bool>();
  m_speed = UnavailableReal();
  PushEventLocked("disconnect", AdapterError::Ok);
  return AdapterResult::Ok();
}

bool ControllerSimulator::IsConnected() const {
  std::lock_guard lock(m_mutex);
  return m_bound && m_link_up;
}

AdapterResult ControllerSimulator::Identify(ControllerIdentity& out) const {
  std::lock_guard lock(m_mutex);
  out = {};
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  out.model = m_profile.model;
  out.serial = m_profile.serial;
  out.firmware = m_profile.firmware;
  out.protocol_version = m_profile.protocol_version;
  out.interface_version = kAdapterInterfaceVersion;
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::GetCapabilities(ControllerCapabilities& out) const {
  std::lock_guard lock(m_mutex);
  out = {};
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  out.carriage = true;
  out.film_axis = true;
  out.light = true;
  out.film_sensor = m_profile.report_film_sensor;
  out.home = true;
  out.encoder = m_profile.has_encoder;
  out.watchdog = true;
  out.watchdog_timeout_known = false;
  out.xray_control = false;
  out.tcp_transport = false;
  out.step = m_profile.step;
  out.carriage_min = m_profile.carriage_min;
  out.carriage_max = m_profile.carriage_max;
  out.film_min = m_profile.film_min;
  out.film_max = m_profile.film_max;
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::GetState(ControllerState& out) const {
  std::lock_guard lock(m_mutex);
  out = BuildStateLocked();
  if (!m_bound) {
    return AdapterResult::Fail(AdapterError::NotConnected, "controller is not connected");
  }
  if (!m_link_up) {
    return AdapterResult::Fail(AdapterError::LinkLost, "controller link is down");
  }
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::Home() {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion == ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Fault, m_fault_code);
  }
  if (m_motion == ControllerMotion::Stopped) {
    return AdapterResult::Fail(AdapterError::Stopped, "stop forbids new motion");
  }
  if (m_motion == ControllerMotion::Scanning || m_armed) {
    return AdapterResult::Fail(AdapterError::Rejected, "home is rejected while a job is armed");
  }
  SetAxisLocked(m_carriage, 0, 0);
  SetAxisLocked(m_film, 0, 0);
  m_referenced = true;
  m_motion = ControllerMotion::Idle;
  ++m_applied_motions;
  PushEventLocked("home", AdapterError::Ok);
  if (m_lose_next_response) {
    m_lose_next_response = false;
    return AdapterResult::Fail(AdapterError::Timeout, "home response lost");
  }
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::Move(const MoveCommand& command) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (command.command_id.empty() || !IsDeviceId(command.command_id)) {
    return AdapterResult::Fail(AdapterError::ParameterRejected, "command_id is required");
  }
  if (m_motion == ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Fault, m_fault_code.empty() ? "fault" : m_fault_code);
  }
  if (m_motion == ControllerMotion::Stopped) {
    return AdapterResult::Fail(AdapterError::Stopped, "stop forbids new motion");
  }
  if (m_motion == ControllerMotion::Scanning || m_armed) {
    return AdapterResult::Fail(AdapterError::Rejected, "move is rejected while a job is armed");
  }
  if (!m_referenced) {
    return AdapterResult::Fail(AdapterError::Rejected, "controller is not referenced");
  }
  if (command.command_id == m_last_command_id) {
    PushEventLocked("move_duplicate", AdapterError::Rejected);
    return AdapterResult::Fail(AdapterError::Rejected, "duplicate command_id does not move again");
  }

  const NumericRange range = command.axis == Axis::Carriage
                                 ? NumericRange{m_profile.carriage_min, m_profile.carriage_max,
                                                m_profile.step, true}
                                 : NumericRange{m_profile.film_min, m_profile.film_max,
                                                m_profile.step, true};
  double accepted = 0;
  if (auto quantized = QuantizeRange(range, command.commanded, accepted); !quantized) {
    if (quantized.code == AdapterError::ParameterRejected) {
      LatchLimitLocked(command.axis, command.commanded > range.maximum);
      m_motion = ControllerMotion::Fault;
      m_fault_code = "limit";
      m_armed = false;
      PushEventLocked("limit", AdapterError::Fault);
      return AdapterResult::Fail(AdapterError::Fault, "move is outside the axis limit");
    }
    return quantized;
  }

  if (auto applied = ApplyPositionLocked(command.axis, command.commanded, accepted); !applied) {
    return applied;
  }
  m_last_command_id = command.command_id;
  m_motion = ControllerMotion::Idle;
  ++m_applied_motions;
  PushEventLocked("move", AdapterError::Ok);
  if (m_lose_next_response) {
    m_lose_next_response = false;
    return AdapterResult::Fail(AdapterError::Timeout, "move response lost");
  }
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::ConfigureScan(const ScanConfig& config) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion == ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Fault, m_fault_code);
  }
  if (m_motion == ControllerMotion::Scanning || m_armed) {
    return AdapterResult::Fail(AdapterError::Rejected, "scan is already armed");
  }
  if (!m_referenced) {
    return AdapterResult::Fail(AdapterError::Rejected, "controller is not referenced");
  }
  if (!IsDeviceId(config.session_id) || !IsDeviceId(config.job_id) || !(config.speed > 0.0)) {
    return AdapterResult::Fail(AdapterError::ParameterRejected, "scan config is incomplete");
  }
  const bool carriage_ok = config.carriage_start >= m_profile.carriage_min &&
                           config.carriage_end <= m_profile.carriage_max &&
                           config.carriage_start <= config.carriage_end;
  const bool film_ok = config.film_start >= m_profile.film_min &&
                       config.film_end <= m_profile.film_max && config.film_start <= config.film_end;
  if (!carriage_ok || !film_ok) {
    return AdapterResult::Fail(AdapterError::ParameterRejected, "scan path is outside the limits");
  }
  m_config.present = true;
  m_config.config = config;
  m_session_id = config.session_id;
  m_job_id = config.job_id;
  m_speed = MakeField(config.speed, FieldOrigin::Commanded);
  m_motion = ControllerMotion::Idle;
  m_armed = false;
  PushEventLocked("configure", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::Arm() {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion == ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Fault, m_fault_code);
  }
  if (m_motion == ControllerMotion::Stopped) {
    return AdapterResult::Fail(AdapterError::Stopped, "stop forbids arming");
  }
  if (!m_config.present || m_motion != ControllerMotion::Idle) {
    return AdapterResult::Fail(AdapterError::Rejected, "scan is not configured");
  }
  m_armed = true;
  m_motion = ControllerMotion::Armed;
  PushEventLocked("arm", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::StartScan(const std::string& job_id) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion == ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Fault, m_fault_code);
  }
  if (m_motion == ControllerMotion::Stopped) {
    return AdapterResult::Fail(AdapterError::Stopped, "stop forbids a new scan");
  }
  if (m_motion == ControllerMotion::Scanning) {
    PushEventLocked("start_duplicate", AdapterError::Rejected);
    return AdapterResult::Fail(AdapterError::Rejected, "scan is already running");
  }
  if (!m_armed || !m_config.present || job_id != m_job_id) {
    return AdapterResult::Fail(AdapterError::Rejected, "scan is not armed for this job");
  }
  m_motion = ControllerMotion::Scanning;
  m_armed = false;
  ++m_applied_motions;
  PushEventLocked("start", AdapterError::Ok);
  if (m_lose_next_response) {
    m_lose_next_response = false;
    return AdapterResult::Fail(AdapterError::Timeout, "start response lost");
  }
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::StopScan() {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion == ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Fault, "stop does not clear a fault");
  }
  m_armed = false;
  m_motion = ControllerMotion::Stopped;
  PushEventLocked("stop", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::AbortScan() {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion != ControllerMotion::Armed && m_motion != ControllerMotion::Scanning &&
      m_motion != ControllerMotion::Idle && m_motion != ControllerMotion::Stopped) {
    return AdapterResult::Fail(AdapterError::Rejected, "nothing to abort");
  }
  if (m_motion == ControllerMotion::Idle && !m_config.present && m_job_id.empty()) {
    return AdapterResult::Fail(AdapterError::Rejected, "nothing to abort");
  }
  m_armed = false;
  m_motion = ControllerMotion::Fault;
  m_fault_code = "abort";
  m_lock_source = "abort";
  PushEventLocked("abort", AdapterError::Fault);
  return AdapterResult::Fail(AdapterError::Fault, "scan aborted");
}

AdapterResult ControllerSimulator::ResetFault() {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_motion != ControllerMotion::Fault) {
    return AdapterResult::Fail(AdapterError::Rejected, "controller is not in fault");
  }
  m_motion = ControllerMotion::Idle;
  m_armed = false;
  m_fault_code.clear();
  m_lock_source.clear();
  m_carriage_positive = KnownFlag(false);
  m_carriage_negative = KnownFlag(false);
  m_film_positive = KnownFlag(false);
  m_film_negative = KnownFlag(false);
  ClearJobLocked();
  PushEventLocked("reset_fault", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::Heartbeat() {
  std::lock_guard lock(m_mutex);
  if (!m_bound) {
    return AdapterResult::Fail(AdapterError::NotConnected, "controller is not connected");
  }
  if (!m_link_up) {
    PushEventLocked("heartbeat", AdapterError::LinkLost);
    return AdapterResult::Fail(AdapterError::LinkLost, "controller link is down");
  }
  return AdapterResult::Ok();
}

AdapterResult ControllerSimulator::SetLight(bool on) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  m_light = KnownFlag(on);
  PushEventLocked(on ? "light_on" : "light_off", AdapterError::Ok);
  return AdapterResult::Ok();
}

void ControllerSimulator::SetOccupied(bool occupied) {
  std::lock_guard lock(m_mutex);
  m_occupied = occupied;
}

void ControllerSimulator::SetLinkUp(bool up) {
  std::lock_guard lock(m_mutex);
  if (!up) {
    m_link_up = false;
    if (m_bound) {
      m_armed = false;
      if (m_motion != ControllerMotion::Fault) {
        m_motion = ControllerMotion::LinkLost;
      }
      PushEventLocked("link_down", AdapterError::LinkLost);
    }
    return;
  }
  const bool was_down = !m_link_up;
  m_link_up = true;
  if (was_down && m_bound) {
    // Линия снова жива. Авария не стирается, задание и движение не продолжаются.
    m_armed = false;
    if (m_motion != ControllerMotion::Fault) {
      m_motion = ControllerMotion::Idle;
      ClearJobLocked();
    }
    PushEventLocked("link_restored", AdapterError::Ok);
  }
}

void ControllerSimulator::ExpireWatchdog() {
  std::lock_guard lock(m_mutex);
  if (!m_bound) {
    return;
  }
  m_armed = false;
  m_motion = ControllerMotion::Fault;
  m_fault_code = "watchdog";
  m_lock_source = "watchdog";
  PushEventLocked("watchdog", AdapterError::Fault);
}

void ControllerSimulator::FailNextResponse() {
  std::lock_guard lock(m_mutex);
  m_lose_next_response = true;
}

AdapterResult ControllerSimulator::CalibrationRelativePath(std::string& path) const {
  std::lock_guard lock(m_mutex);
  if (m_session_id.empty()) {
    path.clear();
    return AdapterResult::Fail(AdapterError::Rejected, "calibration is bound to a session");
  }
  return CalibrationPath(m_profile.scanner_id, m_session_id, path);
}

std::vector<AdapterEvent> ControllerSimulator::Events() const {
  std::lock_guard lock(m_mutex);
  return m_events;
}

int ControllerSimulator::AppliedMotionCount() const {
  std::lock_guard lock(m_mutex);
  return m_applied_motions;
}

AdapterResult ControllerSimulator::RequireLiveLocked() const {
  if (!m_bound) {
    return AdapterResult::Fail(AdapterError::NotConnected, "controller is not connected");
  }
  if (!m_link_up || m_motion == ControllerMotion::LinkLost) {
    return AdapterResult::Fail(AdapterError::LinkLost, "controller link is down");
  }
  return AdapterResult::Ok();
}

ControllerState ControllerSimulator::BuildStateLocked() const {
  ControllerState state;
  state.interface_version = kAdapterInterfaceVersion;
  state.scanner_id = m_profile.scanner_id;
  state.session_id = m_session_id;
  state.job_id = m_job_id;
  state.motion = m_bound ? m_motion : ControllerMotion::Disconnected;
  state.coordinates.carriage = m_carriage;
  state.coordinates.film = m_film;
  state.film_present = m_film_present;
  state.light_on = m_light;
  state.carriage_positive_limit = m_carriage_positive;
  state.carriage_negative_limit = m_carriage_negative;
  state.film_positive_limit = m_film_positive;
  state.film_negative_limit = m_film_negative;
  state.emergency_stop = UnavailableField<bool>();
  state.speed = m_speed;
  state.ready = m_motion == ControllerMotion::Armed || m_motion == ControllerMotion::Scanning;
  state.referenced = m_referenced;
  state.fault_code = m_fault_code;
  state.lock_source = m_lock_source;
  return state;
}

void ControllerSimulator::PushEventLocked(std::string name, AdapterError code) {
  if (m_events.size() >= kEventLimit) {
    m_events_overflow = true;
    return;
  }
  AdapterEvent event;
  event.scanner_id = m_profile.scanner_id;
  event.session_id = m_session_id;
  event.name = std::move(name);
  event.code = code;
  m_events.push_back(std::move(event));
}

void ControllerSimulator::ClearJobLocked() {
  m_config = {};
  m_session_id.clear();
  m_job_id.clear();
  m_speed = UnavailableReal();
}

void ControllerSimulator::SetAxisLocked(AxisSample& axis, double commanded, double accepted) const {
  axis.unit = "step";
  axis.commanded = MakeField(commanded, FieldOrigin::Commanded);
  axis.accepted = MakeField(accepted, FieldOrigin::Accepted);
  if (m_profile.has_encoder) {
    axis.measured = MakeField(accepted + m_profile.encoder_bias, FieldOrigin::Measured);
  } else {
    axis.measured = UnavailableReal();
  }
}

AdapterResult ControllerSimulator::ApplyPositionLocked(Axis axis, double commanded, double accepted) {
  if (axis == Axis::Carriage) {
    SetAxisLocked(m_carriage, commanded, accepted);
  } else {
    SetAxisLocked(m_film, commanded, accepted);
  }
  return AdapterResult::Ok();
}

void ControllerSimulator::LatchLimitLocked(Axis axis, bool positive) {
  m_lock_source = positive ? "limit_positive" : "limit_negative";
  Field<bool> tripped = KnownFlag(true);
  Field<bool> open = KnownFlag(false);
  if (axis == Axis::Carriage) {
    m_carriage_positive = positive ? tripped : open;
    m_carriage_negative = positive ? open : tripped;
  } else {
    m_film_positive = positive ? tripped : open;
    m_film_negative = positive ? open : tripped;
  }
}

}  // namespace tape_scanner::adapters

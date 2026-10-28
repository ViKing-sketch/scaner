#include "tape_scanner/adapters/camera_simulator.hpp"

#include <stdexcept>
#include <utility>

namespace tape_scanner::adapters {
namespace {

constexpr std::size_t kEventLimit = 64;

[[noreturn]] void InvalidProfile(const std::string& message) {
  throw std::invalid_argument(message);
}

void ValidateProfile(const CameraProfile& profile) {
  if (!IsDeviceId(profile.scanner_id) || !IsDeviceId(profile.serial) || profile.model.empty()) {
    InvalidProfile("camera profile needs scanner_id, serial and model");
  }
  if (profile.width <= 0 || profile.height <= 0 || profile.width > profile.max_width ||
      profile.height > profile.max_height || profile.max_width > 8192 || profile.max_height > 8192) {
    InvalidProfile("camera profile frame size is invalid");
  }
  if (UnpackedBytesPerPixel(profile.format) == 0) {
    InvalidProfile("simulator does not emit packed pixel formats");
  }
  bool listed = false;
  for (PixelFormat format : profile.formats) {
    if (UnpackedBytesPerPixel(format) == 0) {
      InvalidProfile("simulator capabilities cannot advertise packed formats");
    }
    if (format == profile.format) {
      listed = true;
    }
  }
  if (!listed) {
    InvalidProfile("active pixel format is not in the capability list");
  }
  double exposure = 0;
  double gain = 0;
  if (!QuantizeRange(profile.exposure_us, profile.default_exposure_us, exposure) ||
      !QuantizeRange(profile.gain, profile.default_gain, gain)) {
    InvalidProfile("default exposure or gain is outside the profile range");
  }
  std::size_t bytes = 0;
  const int stride = profile.width * UnpackedBytesPerPixel(profile.format);
  if (!FrameByteSize(stride, profile.height, bytes)) {
    InvalidProfile("camera frame byte size overflows");
  }
}

}  // namespace

CameraSimulator::CameraSimulator(CameraProfile profile) : m_profile(std::move(profile)) {
  ValidateProfile(m_profile);
  m_format = m_profile.format;
  m_width = m_profile.width;
  m_height = m_profile.height;
  ApplyNominalSettingsLocked();
}

int CameraSimulator::InterfaceVersion() const { return kAdapterInterfaceVersion; }

AdapterResult CameraSimulator::Connect(const CameraEndpoint& endpoint) {
  std::lock_guard lock(m_mutex);
  if (!IsDeviceId(endpoint.serial)) {
    PushEventLocked("connect", AdapterError::ParameterRejected);
    return AdapterResult::Fail(AdapterError::ParameterRejected, "camera serial is required");
  }
  if (endpoint.serial != m_profile.serial ||
      (!m_profile.ip.empty() && endpoint.ip != m_profile.ip)) {
    PushEventLocked("connect", AdapterError::WrongDevice);
    return AdapterResult::Fail(AdapterError::WrongDevice, "camera serial or address does not match");
  }
  if (!m_link_up) {
    PushEventLocked("connect", AdapterError::LinkLost);
    return AdapterResult::Fail(AdapterError::LinkLost, "camera link is down");
  }
  if (m_occupied || m_bound) {
    PushEventLocked("connect", AdapterError::Busy);
    return AdapterResult::Fail(AdapterError::Busy, "camera is occupied");
  }
  m_bound = true;
  m_endpoint_ip = endpoint.ip;
  m_session_id.clear();
  PushEventLocked("connect", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::Disconnect() {
  std::lock_guard lock(m_mutex);
  if (!m_bound) {
    return AdapterResult::Fail(AdapterError::NotConnected, "camera is not connected");
  }
  m_bound = false;
  m_session_id.clear();
  m_endpoint_ip.clear();
  PushEventLocked("disconnect", AdapterError::Ok);
  return AdapterResult::Ok();
}

bool CameraSimulator::IsConnected() const {
  std::lock_guard lock(m_mutex);
  return m_bound && m_link_up;
}

AdapterResult CameraSimulator::Identity(CameraIdentity& out) const {
  std::lock_guard lock(m_mutex);
  out = {};
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  out.model = m_profile.model;
  out.serial = m_profile.serial;
  out.firmware = m_profile.firmware;
  out.endpoint_ip = m_endpoint_ip;
  out.adapter_name = "simulator";
  out.adapter_version = "1";
  out.interface_version = kAdapterInterfaceVersion;
  out.genicam_xml_available = false;
  out.genicam_xml.clear();
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::Capabilities(CameraCapabilities& out) const {
  std::lock_guard lock(m_mutex);
  out = {};
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  out.pixel_formats = m_profile.formats;
  out.exposure_us = m_profile.exposure_us;
  out.gain = m_profile.gain;
  out.max_width = m_profile.max_width;
  out.max_height = m_profile.max_height;
  out.width = m_width;
  out.height = m_height;
  out.exposure_writable = true;
  out.gain_writable = true;
  out.ptp = m_profile.ptp;
  out.action_commands = false;
  out.chunk_data = false;
  out.hardware_trigger = false;
  out.encoder_counter = false;
  out.uses_vendor_sdk = false;
  out.uses_gentl = false;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::SetExposureUs(double requested_us, double& actual_us) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (!m_session_id.empty()) {
    return AdapterResult::Fail(AdapterError::Rejected, "exposure is frozen for the session");
  }
  double quantized = 0;
  if (auto quantized_result = QuantizeRange(m_profile.exposure_us, requested_us, quantized);
      !quantized_result) {
    PushEventLocked("set_exposure", quantized_result.code);
    return quantized_result;
  }
  m_exposure_requested = MakeField(requested_us, FieldOrigin::Commanded);
  m_exposure_actual = MakeField(quantized, FieldOrigin::Device);
  actual_us = quantized;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::SetGain(double requested, double& actual) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (!m_session_id.empty()) {
    return AdapterResult::Fail(AdapterError::Rejected, "gain is frozen for the session");
  }
  double quantized = 0;
  if (auto quantized_result = QuantizeRange(m_profile.gain, requested, quantized); !quantized_result) {
    PushEventLocked("set_gain", quantized_result.code);
    return quantized_result;
  }
  m_gain_requested = MakeField(requested, FieldOrigin::Commanded);
  m_gain_actual = MakeField(quantized, FieldOrigin::Device);
  actual = quantized;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::SetPixelFormat(PixelFormat format, PixelFormat& actual) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (!m_session_id.empty()) {
    return AdapterResult::Fail(AdapterError::Rejected, "pixel format is frozen for the session");
  }
  if (!FormatSupportedLocked(format)) {
    PushEventLocked("set_format", AdapterError::IncompatibleFormat);
    return AdapterResult::Fail(AdapterError::IncompatibleFormat,
                               "pixel format is not supported by this camera");
  }
  m_format = format;
  actual = m_format;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::SetFrameSize(int width, int height, FrameGeometry& actual) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (!m_session_id.empty()) {
    return AdapterResult::Fail(AdapterError::Rejected, "frame size is frozen for the session");
  }
  if (width <= 0 || height <= 0 || width > m_profile.max_width || height > m_profile.max_height) {
    return AdapterResult::Fail(AdapterError::ParameterRejected, "frame size is outside the sensor");
  }
  std::size_t bytes = 0;
  const int stride = width * UnpackedBytesPerPixel(m_format);
  if (!FrameByteSize(stride, height, bytes)) {
    return AdapterResult::Fail(AdapterError::ParameterRejected, "frame size overflows");
  }
  m_width = width;
  m_height = height;
  actual.width = m_width;
  actual.height = m_height;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::GetExposureUs(double& actual_us) const {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (!m_exposure_actual.available) {
    return AdapterResult::Fail(AdapterError::Unavailable, "exposure has not been read back");
  }
  actual_us = m_exposure_actual.value;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::GetGain(double& actual) const {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (!m_gain_actual.available) {
    return AdapterResult::Fail(AdapterError::Unavailable, "gain has not been read back");
  }
  actual = m_gain_actual.value;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::GetPixelFormat(PixelFormat& actual) const {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  actual = m_format;
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::StartSession(const std::string& scanner_id,
                                            const std::string& session_id) {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (scanner_id != m_profile.scanner_id || !IsDeviceId(session_id)) {
    PushEventLocked("start_session", AdapterError::WrongDevice);
    return AdapterResult::Fail(AdapterError::WrongDevice, "session does not belong to this scanner");
  }
  if (!m_session_id.empty()) {
    PushEventLocked("start_session", AdapterError::Busy);
    return AdapterResult::Fail(AdapterError::Busy, "camera session is already open");
  }
  m_session_id = session_id;
  PushEventLocked("start_session", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::StopSession() {
  std::lock_guard lock(m_mutex);
  if (auto live = RequireLiveLocked(); !live) {
    return live;
  }
  if (m_session_id.empty()) {
    return AdapterResult::Fail(AdapterError::Rejected, "camera session is not open");
  }
  m_session_id.clear();
  PushEventLocked("stop_session", AdapterError::Ok);
  return AdapterResult::Ok();
}

AdapterResult CameraSimulator::Capture(const CaptureRequest& request, Frame& out) {
  std::lock_guard lock(m_mutex);
  out = ClearedFrame();
  if (!m_bound || !m_link_up) {
    out.scanner_id = m_profile.scanner_id;
    out.error = AdapterError::LinkLost;
    out.status = m_bound ? AcquisitionStatus::LinkLost : AcquisitionStatus::None;
    if (!m_bound) {
      out.error = AdapterError::NotConnected;
      out.status_detail = "camera is not connected";
      return AdapterResult::Fail(AdapterError::NotConnected, out.status_detail);
    }
    out.status_detail = "camera link is down";
    PushEventLocked("capture", AdapterError::LinkLost);
    return AdapterResult::Fail(AdapterError::LinkLost, out.status_detail);
  }
  if (request.scanner_id != m_profile.scanner_id) {
    out.error = AdapterError::WrongDevice;
    out.status_detail = "capture scanner_id does not match this camera";
    PushEventLocked("capture", AdapterError::WrongDevice);
    return AdapterResult::Fail(AdapterError::WrongDevice, out.status_detail);
  }
  if (m_session_id.empty() || request.session_id != m_session_id) {
    out.scanner_id = m_profile.scanner_id;
    out.error = AdapterError::Rejected;
    out.status_detail = "capture session does not match the open session";
    PushEventLocked("capture", AdapterError::Rejected);
    return AdapterResult::Fail(AdapterError::Rejected, out.status_detail);
  }

  out.scanner_id = m_profile.scanner_id;
  out.session_id = m_session_id;
  out.tile_id = request.tile_id;
  out.exposure_id = request.exposure_id;
  out.coordinates = request.coordinates;
  out.width = m_width;
  out.height = m_height;
  out.format = m_format;
  out.significant_bits = SignificantBits(m_format);
  out.stride = m_width * UnpackedBytesPerPixel(m_format);
  FillExposureFieldsLocked(out);
  out.host_timestamp_ns = MakeField(NowLocked(), FieldOrigin::Host);
  if (m_profile.ptp && out.host_timestamp_ns.available && out.host_timestamp_ns.value >= 0) {
    out.device_timestamp_ns =
        MakeField(static_cast<std::uint64_t>(out.host_timestamp_ns.value), FieldOrigin::Device);
  }

  if (!m_faults.empty()) {
    const CameraFaultKind kind = m_faults.front();
    m_faults.erase(m_faults.begin());
    out.buffer = {};
    switch (kind) {
    case CameraFaultKind::LinkLost:
      m_link_up = false;
      m_session_id.clear();
      out.status = AcquisitionStatus::LinkLost;
      out.error = AdapterError::LinkLost;
      out.status_detail = "camera link lost";
      break;
    case CameraFaultKind::Timeout:
      out.status = AcquisitionStatus::Timeout;
      out.error = AdapterError::Timeout;
      out.status_detail = "camera capture timed out";
      break;
    case CameraFaultKind::Fault:
      out.status = AcquisitionStatus::Fault;
      out.error = AdapterError::Fault;
      out.status_detail = "camera fault";
      break;
    case CameraFaultKind::Dropped:
      out.status = AcquisitionStatus::Dropped;
      out.error = AdapterError::Dropped;
      out.status_detail = "camera frame dropped";
      break;
    case CameraFaultKind::QueueOverflow:
      out.status = AcquisitionStatus::QueueOverflow;
      out.error = AdapterError::QueueOverflow;
      out.status_detail = "camera queue is full";
      break;
    }
    PushEventLocked("capture", out.error);
    return AdapterResult::Fail(out.error, out.status_detail);
  }

  out.buffer = SealBuffer(RenderLocked(request.tile_id, request.exposure_id));
  out.status = AcquisitionStatus::Ok;
  out.error = AdapterError::Ok;
  ++m_completed;
  PushEventLocked("capture", AdapterError::Ok);
  return AdapterResult::Ok();
}

void CameraSimulator::SetOccupied(bool occupied) {
  std::lock_guard lock(m_mutex);
  m_occupied = occupied;
}

void CameraSimulator::SetLinkUp(bool up) {
  std::lock_guard lock(m_mutex);
  m_link_up = up;
  if (!up) {
    m_session_id.clear();
    PushEventLocked("link_down", AdapterError::LinkLost);
  } else {
    PushEventLocked("link_up", AdapterError::Ok);
  }
}

void CameraSimulator::QueueFault(CameraFaultKind kind) {
  std::lock_guard lock(m_mutex);
  m_faults.push_back(kind);
}

void CameraSimulator::SetClock(std::function<std::int64_t()> clock) {
  std::lock_guard lock(m_mutex);
  m_clock = std::move(clock);
}

AdapterResult CameraSimulator::CalibrationRelativePath(std::string& path) const {
  std::lock_guard lock(m_mutex);
  if (m_session_id.empty()) {
    path.clear();
    return AdapterResult::Fail(AdapterError::Rejected, "calibration is bound to an open session");
  }
  return CalibrationPath(m_profile.scanner_id, m_session_id, path);
}

std::vector<AdapterEvent> CameraSimulator::Events() const {
  std::lock_guard lock(m_mutex);
  return m_events;
}

bool CameraSimulator::EventsOverflowed() const {
  std::lock_guard lock(m_mutex);
  return m_events_overflow;
}

int CameraSimulator::CompletedCaptures() const {
  std::lock_guard lock(m_mutex);
  return m_completed;
}

AdapterResult CameraSimulator::RequireLiveLocked() const {
  if (!m_bound) {
    return AdapterResult::Fail(AdapterError::NotConnected, "camera is not connected");
  }
  if (!m_link_up) {
    return AdapterResult::Fail(AdapterError::LinkLost, "camera link is down");
  }
  return AdapterResult::Ok();
}

bool CameraSimulator::FormatSupportedLocked(PixelFormat format) const {
  if (UnpackedBytesPerPixel(format) == 0) {
    return false;
  }
  for (PixelFormat candidate : m_profile.formats) {
    if (candidate == format) {
      return true;
    }
  }
  return false;
}

void CameraSimulator::PushEventLocked(std::string name, AdapterError code) {
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

void CameraSimulator::ApplyNominalSettingsLocked() {
  double exposure = 0;
  double gain = 0;
  if (!QuantizeRange(m_profile.exposure_us, m_profile.default_exposure_us, exposure) ||
      !QuantizeRange(m_profile.gain, m_profile.default_gain, gain)) {
    InvalidProfile("default exposure or gain is outside the profile range");
  }
  m_exposure_requested = MakeField(m_profile.default_exposure_us, FieldOrigin::Commanded);
  m_exposure_actual = MakeField(exposure, FieldOrigin::Device);
  m_gain_requested = MakeField(m_profile.default_gain, FieldOrigin::Commanded);
  m_gain_actual = MakeField(gain, FieldOrigin::Device);
}

void CameraSimulator::FillExposureFieldsLocked(Frame& frame) const {
  frame.exposure_requested_us = m_exposure_requested;
  frame.exposure_us = m_exposure_actual;
  frame.gain_requested = m_gain_requested;
  frame.gain = m_gain_actual;
}

std::vector<std::uint8_t> CameraSimulator::RenderLocked(std::uint64_t tile_id,
                                                        std::uint64_t exposure_id) const {
  const int bpp = UnpackedBytesPerPixel(m_format);
  const int stride = m_width * bpp;
  std::size_t bytes = 0;
  if (!FrameByteSize(stride, m_height, bytes)) {
    return {};
  }
  std::vector<std::uint8_t> pixels(bytes);
  const int bits = SignificantBits(m_format);
  for (int y = 0; y < m_height; ++y) {
    for (int x = 0; x < m_width; ++x) {
      const std::uint32_t pixel = SimulatorPixel(tile_id, exposure_id, x, y, bits);
      const std::size_t index = static_cast<std::size_t>(y * stride + x * bpp);
      pixels[index] = static_cast<std::uint8_t>(pixel & 0xffu);
      if (bpp == 2) {
        pixels[index + 1] = static_cast<std::uint8_t>((pixel >> 8) & 0xffu);
      }
    }
  }
  return pixels;
}

std::int64_t CameraSimulator::NowLocked() {
  if (m_clock) {
    return m_clock();
  }
  m_clock_ns += 1'000'000;
  return m_clock_ns;
}

}  // namespace tape_scanner::adapters

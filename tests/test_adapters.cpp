#include "tape_scanner/adapters/camera_simulator.hpp"
#include "tape_scanner/adapters/controller_simulator.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char* expr, int line) {
  ++g_checks;
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

bool Near(double a, double b) { return std::abs(a - b) < 1e-6; }

using tape_scanner::adapters::AcquisitionStatus;
using tape_scanner::adapters::AdapterError;
using tape_scanner::adapters::AdapterResult;
using tape_scanner::adapters::Axis;
using tape_scanner::adapters::CalibrationPath;
using tape_scanner::adapters::CameraEndpoint;
using tape_scanner::adapters::CameraFaultKind;
using tape_scanner::adapters::CameraProfile;
using tape_scanner::adapters::CameraSimulator;
using tape_scanner::adapters::CaptureRequest;
using tape_scanner::adapters::ControllerEndpoint;
using tape_scanner::adapters::ControllerMotion;
using tape_scanner::adapters::ControllerProfile;
using tape_scanner::adapters::ControllerSimulator;
using tape_scanner::adapters::FieldOrigin;
using tape_scanner::adapters::Frame;
using tape_scanner::adapters::FrameGeometry;
using tape_scanner::adapters::ICamera;
using tape_scanner::adapters::IController;
using tape_scanner::adapters::IsGlobalCoefsName;
using tape_scanner::adapters::IsUnavailableReal;
using tape_scanner::adapters::MakeCameraProfile;
using tape_scanner::adapters::MakeControllerProfile;
using tape_scanner::adapters::MoveCommand;
using tape_scanner::adapters::PixelFormat;
using tape_scanner::adapters::ScanConfig;
using tape_scanner::adapters::SealBuffer;
using tape_scanner::adapters::SimulatorPixel;
using tape_scanner::adapters::kAdapterInterfaceVersion;

void Expect(const AdapterResult& result, AdapterError code, int line) {
  ++g_checks;
  if (result.code != code) {
    std::cerr << "FAIL line " << line << ": error " << static_cast<int>(result.code)
              << " != " << static_cast<int>(code) << " (" << result.message << ")\n";
    ++g_failures;
  }
}

#define EXPECT(result, code) Expect((result), (code), __LINE__)

std::uint16_t ReadLe16(const Frame& frame, int x, int y) {
  const std::size_t index = static_cast<std::size_t>(y * frame.stride + x * 2);
  return static_cast<std::uint16_t>(frame.buffer.data()[index] |
                                     (frame.buffer.data()[index + 1] << 8));
}

CameraProfile SmallCamera(std::string scanner, std::string serial, PixelFormat format, int width,
                          int height) {
  CameraProfile profile = MakeCameraProfile(std::move(scanner), std::move(serial));
  profile.width = width;
  profile.height = height;
  profile.max_width = 1280;
  profile.max_height = 960;
  profile.format = format;
  return profile;
}

ControllerProfile SmallController(std::string scanner, std::string serial) {
  ControllerProfile profile = MakeControllerProfile(std::move(scanner), std::move(serial));
  profile.carriage_min = 0;
  profile.carriage_max = 100;
  profile.film_min = 0;
  profile.film_max = 1000;
  return profile;
}

void TestContractHelpers() {
  std::vector<std::uint8_t> raw{1, 2, 3};
  const auto sealed = SealBuffer(raw);
  raw[0] = 9;
  CHECK(sealed.size() == 3);
  CHECK(sealed.data()[0] == 1);
  const auto alias = sealed;
  CHECK(alias.data() == sealed.data());

  std::string path_a;
  std::string path_b;
  EXPECT(CalibrationPath("scn-a", "ses-a", path_a), AdapterError::Ok);
  EXPECT(CalibrationPath("scn-b", "ses-b", path_b), AdapterError::Ok);
  CHECK(path_a == "calibrations/scn-a/ses-a/coefs.xml");
  CHECK(path_b == "calibrations/scn-b/ses-b/coefs.xml");
  CHECK(path_a != path_b);
  CHECK(IsGlobalCoefsName("coefs.xml"));
  CHECK(IsGlobalCoefsName("calibrations/coefs.xml"));
  CHECK(!IsGlobalCoefsName(path_a));
  std::string rejected;
  EXPECT(CalibrationPath("", "ses-a", rejected), AdapterError::ParameterRejected);
  EXPECT(CalibrationPath("scn/a", "ses-a", rejected), AdapterError::ParameterRejected);
  CHECK(rejected.empty());

  CHECK(MakeCameraProfile("bad id", "CAM-A").serial == "CAM-A");
  bool threw = false;
  try {
    CameraSimulator broken(MakeCameraProfile("bad id", "CAM-A"));
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

void TestNominalFrame() {
  CameraSimulator camera(SmallCamera("scn-a", "CAM-A", PixelFormat::Mono16, 4, 2));
  std::int64_t now = 5'000;
  camera.SetClock([&now] { return now; });
  ICamera& device = camera;

  CHECK(device.InterfaceVersion() == kAdapterInterfaceVersion);
  EXPECT(device.Connect({}), AdapterError::ParameterRejected);
  EXPECT(device.Connect({"CAM-B", ""}), AdapterError::WrongDevice);
  CHECK(!device.IsConnected());
  EXPECT(device.Connect({"CAM-A", "10.0.0.8"}), AdapterError::Ok);

  tape_scanner::adapters::CameraIdentity identity;
  EXPECT(device.Identity(identity), AdapterError::Ok);
  CHECK(identity.serial == "CAM-A");
  CHECK(identity.model == "sim-gige");
  CHECK(identity.adapter_name == "simulator");
  CHECK(identity.adapter_name.find("MVCAM") == std::string::npos);
  CHECK(identity.interface_version == kAdapterInterfaceVersion);
  CHECK(!identity.genicam_xml_available);
  CHECK(identity.genicam_xml.empty());
  CHECK(identity.endpoint_ip == "10.0.0.8");

  tape_scanner::adapters::CameraCapabilities caps;
  EXPECT(device.Capabilities(caps), AdapterError::Ok);
  CHECK(!caps.uses_vendor_sdk);
  CHECK(!caps.uses_gentl);
  CHECK(!caps.ptp);
  CHECK(!caps.hardware_trigger);
  CHECK(!caps.chunk_data);

  double actual_exposure = 0;
  EXPECT(device.SetExposureUs(100.4, actual_exposure), AdapterError::Ok);
  CHECK(Near(actual_exposure, 100));
  double read_back = 0;
  EXPECT(device.GetExposureUs(read_back), AdapterError::Ok);
  CHECK(Near(read_back, 100));
  EXPECT(device.SetExposureUs(10, actual_exposure), AdapterError::ParameterRejected);
  EXPECT(device.GetExposureUs(read_back), AdapterError::Ok);
  CHECK(Near(read_back, 100));

  double actual_gain = 0;
  EXPECT(device.SetGain(2.04, actual_gain), AdapterError::Ok);
  CHECK(Near(actual_gain, 2.0));

  PixelFormat actual_format = PixelFormat::Unknown;
  EXPECT(device.SetPixelFormat(PixelFormat::Mono12Packed, actual_format),
         AdapterError::IncompatibleFormat);
  EXPECT(device.GetPixelFormat(actual_format), AdapterError::Ok);
  CHECK(actual_format == PixelFormat::Mono16);

  EXPECT(device.StartSession("scn-b", "ses-a"), AdapterError::WrongDevice);
  EXPECT(device.StartSession("scn-a", "ses-a"), AdapterError::Ok);
  EXPECT(device.SetExposureUs(200, actual_exposure), AdapterError::Rejected);
  EXPECT(device.GetExposureUs(read_back), AdapterError::Ok);
  CHECK(Near(read_back, 100));

  Frame foreign;
  CaptureRequest wrong = {"scn-b", "ses-a", 1, 1, {}};
  EXPECT(device.Capture(wrong, foreign), AdapterError::WrongDevice);
  CHECK(foreign.buffer.empty());
  CHECK(foreign.scanner_id.empty());

  CaptureRequest request;
  request.scanner_id = "scn-a";
  request.session_id = "ses-b";
  request.tile_id = 7;
  request.exposure_id = 3;
  Frame rejected;
  EXPECT(device.Capture(request, rejected), AdapterError::Rejected);
  CHECK(rejected.buffer.empty());
  CHECK(camera.CompletedCaptures() == 0);

  request.session_id = "ses-a";
  request.coordinates.carriage.commanded =
      tape_scanner::adapters::MakeField(10.4, FieldOrigin::Commanded);
  request.coordinates.carriage.accepted =
      tape_scanner::adapters::MakeField(10.0, FieldOrigin::Accepted);
  request.coordinates.carriage.measured =
      tape_scanner::adapters::UnavailableReal();
  Frame frame;
  EXPECT(device.Capture(request, frame), AdapterError::Ok);
  CHECK(frame.interface_version == kAdapterInterfaceVersion);
  CHECK(frame.scanner_id == "scn-a");
  CHECK(frame.session_id == "ses-a");
  CHECK(frame.tile_id == 7);
  CHECK(frame.exposure_id == 3);
  CHECK(frame.width == 4);
  CHECK(frame.height == 2);
  CHECK(frame.stride == 8);
  CHECK(frame.format == PixelFormat::Mono16);
  CHECK(frame.significant_bits == 16);
  CHECK(frame.status == AcquisitionStatus::Ok);
  CHECK(frame.buffer.size() == 16);
  CHECK(frame.exposure_requested_us.available);
  CHECK(frame.exposure_requested_us.origin == FieldOrigin::Commanded);
  CHECK(Near(frame.exposure_requested_us.value, 100.4));
  CHECK(frame.exposure_us.available);
  CHECK(frame.exposure_us.origin == FieldOrigin::Device);
  CHECK(Near(frame.exposure_us.value, 100));
  CHECK(Near(frame.gain.value, 2.0));
  CHECK(frame.host_timestamp_ns.available);
  CHECK(frame.host_timestamp_ns.origin == FieldOrigin::Host);
  CHECK(frame.host_timestamp_ns.value == 5'000);
  CHECK(!frame.device_timestamp_ns.available);
  CHECK(frame.device_timestamp_ns.origin == FieldOrigin::Unavailable);
  CHECK(Near(frame.coordinates.carriage.commanded.value, 10.4));
  CHECK(Near(frame.coordinates.carriage.accepted.value, 10.0));
  CHECK(IsUnavailableReal(frame.coordinates.carriage.measured));

  const auto* first = frame.buffer.data();
  for (int y = 0; y < frame.height; ++y) {
    for (int x = 0; x < frame.width; ++x) {
      CHECK(ReadLe16(frame, x, y) == SimulatorPixel(7, 3, x, y, 16));
    }
  }

  now = 9'000;
  request.exposure_id = 4;
  Frame second;
  EXPECT(device.Capture(request, second), AdapterError::Ok);
  CHECK(frame.buffer.data() == first);
  CHECK(ReadLe16(frame, 0, 0) == SimulatorPixel(7, 3, 0, 0, 16));
  CHECK(ReadLe16(second, 0, 0) == SimulatorPixel(7, 4, 0, 0, 16));
  CHECK(second.host_timestamp_ns.value == 9'000);
  CHECK(frame.host_timestamp_ns.value == 5'000);

  std::string calibration;
  EXPECT(camera.CalibrationRelativePath(calibration), AdapterError::Ok);
  CHECK(calibration == "calibrations/scn-a/ses-a/coefs.xml");
  CHECK(!IsGlobalCoefsName(calibration));

  CameraProfile addressed = SmallCamera("scn-ip", "CAM-IP", PixelFormat::Mono8, 2, 1);
  addressed.ip = "192.168.1.10";
  CameraSimulator by_address(addressed);
  EXPECT(by_address.Connect({"CAM-IP", "10.0.0.1"}), AdapterError::WrongDevice);
  EXPECT(by_address.Connect({"CAM-IP", ""}), AdapterError::WrongDevice);
  CHECK(!by_address.IsConnected());
  EXPECT(by_address.Connect({"CAM-OTHER", "192.168.1.10"}), AdapterError::WrongDevice);
  EXPECT(by_address.Connect({"CAM-IP", "192.168.1.10"}), AdapterError::Ok);
  CHECK(by_address.IsConnected());
  tape_scanner::adapters::CameraIdentity addressed_id;
  EXPECT(by_address.Identity(addressed_id), AdapterError::Ok);
  CHECK(addressed_id.serial == "CAM-IP");
  CHECK(addressed_id.endpoint_ip == "192.168.1.10");
}

void TestFormatsAndFaultFrames() {
  CameraSimulator camera(SmallCamera("scn-a", "CAM-A", PixelFormat::Mono16, 8, 2));
  CHECK(camera.Connect({"CAM-A", ""}));
  PixelFormat format = PixelFormat::Unknown;
  EXPECT(camera.SetPixelFormat(PixelFormat::Mono12, format), AdapterError::Ok);
  FrameGeometry size;
  EXPECT(camera.SetFrameSize(3, 2, size), AdapterError::Ok);
  CHECK(size.width == 3);
  CHECK(size.height == 2);
  EXPECT(camera.StartSession("scn-a", "ses-a"), AdapterError::Ok);

  CaptureRequest request{"scn-a", "ses-a", 1, 2, {}};
  Frame mono12;
  EXPECT(camera.Capture(request, mono12), AdapterError::Ok);
  CHECK(mono12.format == PixelFormat::Mono12);
  CHECK(mono12.significant_bits == 12);
  CHECK(mono12.stride == 6);
  CHECK(mono12.buffer.size() == 12);
  for (int y = 0; y < 2; ++y) {
    for (int x = 0; x < 3; ++x) {
      const auto pixel = ReadLe16(mono12, x, y);
      CHECK(pixel == SimulatorPixel(1, 2, x, y, 12));
      CHECK(pixel <= 0x0fff);
    }
  }

  camera.QueueFault(CameraFaultKind::Dropped);
  camera.QueueFault(CameraFaultKind::Timeout);
  camera.QueueFault(CameraFaultKind::QueueOverflow);
  camera.QueueFault(CameraFaultKind::Fault);
  Frame dropped;
  EXPECT(camera.Capture(request, dropped), AdapterError::Dropped);
  CHECK(dropped.buffer.empty());
  CHECK(dropped.status == AcquisitionStatus::Dropped);
  CHECK(dropped.scanner_id == "scn-a");
  CHECK(dropped.session_id == "ses-a");
  CHECK(dropped.tile_id == 1);
  CHECK(camera.IsConnected());
  CHECK(mono12.buffer.size() == 12);

  Frame timeout;
  EXPECT(camera.Capture(request, timeout), AdapterError::Timeout);
  CHECK(timeout.buffer.empty());
  CHECK(timeout.status == AcquisitionStatus::Timeout);
  CHECK(camera.IsConnected());

  Frame overflow;
  EXPECT(camera.Capture(request, overflow), AdapterError::QueueOverflow);
  CHECK(overflow.buffer.empty());
  CHECK(overflow.status == AcquisitionStatus::QueueOverflow);

  Frame fault;
  EXPECT(camera.Capture(request, fault), AdapterError::Fault);
  CHECK(fault.status == AcquisitionStatus::Fault);
  CHECK(fault.buffer.empty());

  Frame recovered;
  EXPECT(camera.Capture(request, recovered), AdapterError::Ok);
  CHECK(recovered.buffer.size() == 12);

  camera.QueueFault(CameraFaultKind::LinkLost);
  Frame lost;
  EXPECT(camera.Capture(request, lost), AdapterError::LinkLost);
  CHECK(lost.buffer.empty());
  CHECK(lost.session_id == "ses-a");
  CHECK(!camera.IsConnected());
  Frame still_lost;
  EXPECT(camera.Capture(request, still_lost), AdapterError::LinkLost);
  CHECK(still_lost.buffer.empty());
  camera.SetLinkUp(true);
  EXPECT(camera.Capture(request, still_lost), AdapterError::Rejected);
  EXPECT(camera.StartSession("scn-a", "ses-a"), AdapterError::Ok);
  EXPECT(camera.Capture(request, recovered), AdapterError::Ok);
}

void TestReferenceFrameSize() {
  CameraSimulator camera(MakeCameraProfile("scn-ref", "CAM-REF"));
  CHECK(camera.Profile().width == 1280);
  CHECK(camera.Profile().height == 960);
  CHECK(camera.Connect({"CAM-REF", ""}));
  CHECK(camera.StartSession("scn-ref", "ses-ref"));
  Frame frame;
  EXPECT(camera.Capture({"scn-ref", "ses-ref", 0, 1, {}}, frame), AdapterError::Ok);
  CHECK(frame.width == 1280);
  CHECK(frame.height == 960);
  CHECK(frame.stride == 2560);
  CHECK(frame.buffer.size() == 1280u * 960u * 2u);
  CHECK(ReadLe16(frame, 0, 0) == SimulatorPixel(0, 1, 0, 0, 16));
  CHECK(ReadLe16(frame, 1279, 959) == SimulatorPixel(0, 1, 1279, 959, 16));
}

void TestTwoScanners() {
  CameraSimulator camera_a(SmallCamera("scn-a", "CAM-A", PixelFormat::Mono16, 4, 2));
  CameraProfile profile_b = MakeCameraProfile("scn-b", "CAM-B");
  profile_b.width = 4;
  profile_b.height = 2;
  profile_b.max_width = 1280;
  profile_b.max_height = 960;
  profile_b.format = PixelFormat::Mono8;
  profile_b.ptp = true;
  CameraSimulator ptp_b(profile_b);

  ControllerSimulator controller_a(SmallController("scn-a", "CTL-A"));
  ControllerProfile profile_ctl_b = SmallController("scn-b", "CTL-B");
  profile_ctl_b.has_encoder = true;
  profile_ctl_b.encoder_bias = 0.5;
  profile_ctl_b.report_film_sensor = false;
  ControllerSimulator controller_b(profile_ctl_b);

  EXPECT(camera_a.Connect({"CAM-B", ""}), AdapterError::WrongDevice);
  EXPECT(camera_a.Connect({"CAM-A", ""}), AdapterError::Ok);
  EXPECT(camera_a.Connect({"CAM-B", ""}), AdapterError::WrongDevice);
  CHECK(camera_a.IsConnected());
  EXPECT(camera_a.Connect({"CAM-A", ""}), AdapterError::Busy);
  CHECK(camera_a.IsConnected());

  camera_a.SetOccupied(true);
  CameraSimulator occupied(SmallCamera("scn-a", "CAM-A", PixelFormat::Mono16, 2, 2));
  // Occupied is per instance: the second object is a second simulated device.
  occupied.SetOccupied(true);
  EXPECT(occupied.Connect({"CAM-A", ""}), AdapterError::Busy);
  CHECK(!occupied.IsConnected());
  EXPECT(ptp_b.Connect({"CAM-B", ""}), AdapterError::Ok);

  EXPECT(controller_a.Connect({"CTL-B"}), AdapterError::WrongDevice);
  EXPECT(controller_a.Connect({"CTL-A"}), AdapterError::Ok);
  EXPECT(controller_b.Connect({"CTL-B"}), AdapterError::Ok);
  EXPECT(controller_a.Connect({"CTL-A"}), AdapterError::Busy);

  tape_scanner::adapters::ControllerState before;
  EXPECT(controller_a.GetState(before), AdapterError::Ok);
  CHECK(before.scanner_id == "scn-a");
  CHECK(!before.referenced);
  CHECK(IsUnavailableReal(before.coordinates.carriage.accepted));
  CHECK(IsUnavailableReal(before.coordinates.carriage.measured));
  CHECK(!before.emergency_stop.available);
  CHECK(before.emergency_stop.origin == FieldOrigin::Unavailable);
  CHECK(before.film_present.available);
  CHECK(!before.film_present.value);

  tape_scanner::adapters::ControllerCapabilities caps_b;
  EXPECT(controller_b.GetCapabilities(caps_b), AdapterError::Ok);
  CHECK(caps_b.encoder);
  CHECK(!caps_b.watchdog_timeout_known);
  CHECK(!caps_b.xray_control);
  CHECK(!caps_b.tcp_transport);
  tape_scanner::adapters::ControllerIdentity identity_b;
  EXPECT(controller_b.Identify(identity_b), AdapterError::Ok);
  CHECK(identity_b.serial == "CTL-B");
  CHECK(identity_b.protocol_version == "logical-1");
  CHECK(identity_b.interface_version == kAdapterInterfaceVersion);

  tape_scanner::adapters::ControllerState sensor_b;
  EXPECT(controller_b.GetState(sensor_b), AdapterError::Ok);
  CHECK(!sensor_b.film_present.available);
  CHECK(sensor_b.film_present.origin == FieldOrigin::Unavailable);

  EXPECT(controller_a.Home(), AdapterError::Ok);
  EXPECT(controller_b.Home(), AdapterError::Ok);
  tape_scanner::adapters::ControllerState home_a;
  tape_scanner::adapters::ControllerState home_b;
  EXPECT(controller_a.GetState(home_a), AdapterError::Ok);
  EXPECT(controller_b.GetState(home_b), AdapterError::Ok);
  CHECK(home_a.referenced);
  CHECK(Near(home_a.coordinates.carriage.accepted.value, 0));
  CHECK(home_a.coordinates.carriage.accepted.origin == FieldOrigin::Accepted);
  CHECK(IsUnavailableReal(home_a.coordinates.carriage.measured));
  CHECK(Near(home_b.coordinates.carriage.measured.value, 0.5));
  CHECK(home_b.coordinates.carriage.measured.origin == FieldOrigin::Measured);

  const int motions_a = controller_a.AppliedMotionCount();
  EXPECT(controller_a.Move({"m-out", Axis::Carriage, 100.4}), AdapterError::Fault);
  tape_scanner::adapters::ControllerState limited;
  EXPECT(controller_a.GetState(limited), AdapterError::Ok);
  CHECK(limited.motion == ControllerMotion::Fault);
  CHECK(limited.fault_code == "limit");
  CHECK(limited.lock_source == "limit_positive");
  CHECK(limited.carriage_positive_limit.available);
  CHECK(limited.carriage_positive_limit.value);
  CHECK(Near(limited.coordinates.carriage.accepted.value, 0));
  CHECK(controller_a.AppliedMotionCount() == motions_a);
  EXPECT(controller_b.GetState(home_b), AdapterError::Ok);
  CHECK(home_b.motion == ControllerMotion::Idle);
  CHECK(Near(home_b.coordinates.carriage.accepted.value, 0));

  EXPECT(controller_a.ResetFault(), AdapterError::Ok);
  tape_scanner::adapters::ControllerState cleared;
  EXPECT(controller_a.GetState(cleared), AdapterError::Ok);
  CHECK(cleared.motion == ControllerMotion::Idle);
  CHECK(cleared.job_id.empty());
  CHECK(!cleared.ready);
  CHECK(Near(cleared.coordinates.carriage.accepted.value, 0));

  controller_a.FailNextResponse();
  EXPECT(controller_a.Move({"m1", Axis::Carriage, 10.4}), AdapterError::Timeout);
  tape_scanner::adapters::ControllerState moved;
  EXPECT(controller_a.GetState(moved), AdapterError::Ok);
  CHECK(Near(moved.coordinates.carriage.commanded.value, 10.4));
  CHECK(moved.coordinates.carriage.commanded.origin == FieldOrigin::Commanded);
  CHECK(Near(moved.coordinates.carriage.accepted.value, 10));
  CHECK(IsUnavailableReal(moved.coordinates.carriage.measured));
  const int after_lost = controller_a.AppliedMotionCount();
  EXPECT(controller_a.Move({"m1", Axis::Carriage, 50}), AdapterError::Rejected);
  EXPECT(controller_a.GetState(moved), AdapterError::Ok);
  CHECK(Near(moved.coordinates.carriage.accepted.value, 10));
  CHECK(controller_a.AppliedMotionCount() == after_lost);
  EXPECT(controller_a.Move({"m2", Axis::Film, 4}), AdapterError::Ok);

  EXPECT(controller_b.Move({"b1", Axis::Carriage, 20}), AdapterError::Ok);
  EXPECT(controller_b.GetState(home_b), AdapterError::Ok);
  CHECK(Near(home_b.coordinates.carriage.accepted.value, 20));
  CHECK(Near(home_b.coordinates.carriage.measured.value, 20.5));
  CHECK(Near(moved.coordinates.carriage.accepted.value, 10));

  ScanConfig config_a;
  config_a.session_id = "ses-a";
  config_a.job_id = "job-a";
  config_a.carriage_start = 0;
  config_a.carriage_end = 10;
  config_a.film_start = 0;
  config_a.film_end = 4;
  config_a.speed = 1;
  EXPECT(controller_a.ConfigureScan(config_a), AdapterError::Ok);
  EXPECT(controller_a.Arm(), AdapterError::Ok);
  EXPECT(controller_a.StartScan("job-b"), AdapterError::Rejected);
  EXPECT(controller_a.StartScan("job-a"), AdapterError::Ok);
  EXPECT(controller_a.StartScan("job-a"), AdapterError::Rejected);
  tape_scanner::adapters::ControllerState scanning;
  EXPECT(controller_a.GetState(scanning), AdapterError::Ok);
  CHECK(scanning.motion == ControllerMotion::Scanning);
  CHECK(scanning.ready);
  CHECK(scanning.session_id == "ses-a");
  CHECK(scanning.job_id == "job-a");

  std::string path_a;
  std::string path_b;
  EXPECT(controller_a.CalibrationRelativePath(path_a), AdapterError::Ok);
  ScanConfig config_b = config_a;
  config_b.session_id = "ses-b";
  config_b.job_id = "job-b";
  EXPECT(controller_b.ConfigureScan(config_b), AdapterError::Ok);
  EXPECT(controller_b.CalibrationRelativePath(path_b), AdapterError::Ok);
  CHECK(path_a == "calibrations/scn-a/ses-a/coefs.xml");
  CHECK(path_b == "calibrations/scn-b/ses-b/coefs.xml");
  CHECK(path_a != path_b);

  const double carriage_a = scanning.coordinates.carriage.accepted.value;
  const double carriage_b = home_b.coordinates.carriage.accepted.value;
  controller_a.SetLinkUp(false);
  EXPECT(controller_a.Heartbeat(), AdapterError::LinkLost);
  EXPECT(controller_a.Move({"m3", Axis::Carriage, 1}), AdapterError::LinkLost);
  tape_scanner::adapters::ControllerState lost;
  EXPECT(controller_a.GetState(lost), AdapterError::LinkLost);
  CHECK(lost.motion == ControllerMotion::LinkLost);
  CHECK(Near(lost.coordinates.carriage.accepted.value, carriage_a));
  EXPECT(controller_b.GetState(home_b), AdapterError::Ok);
  CHECK(home_b.motion == ControllerMotion::Idle);
  CHECK(Near(home_b.coordinates.carriage.accepted.value, carriage_b));
  controller_a.SetLinkUp(true);
  EXPECT(controller_a.GetState(lost), AdapterError::Ok);
  CHECK(lost.motion == ControllerMotion::Idle);
  CHECK(lost.job_id.empty());
  CHECK(!lost.ready);
  CHECK(Near(lost.coordinates.carriage.accepted.value, carriage_a));
  EXPECT(controller_a.StartScan("job-a"), AdapterError::Rejected);

  EXPECT(controller_a.Home(), AdapterError::Ok);
  EXPECT(controller_a.ConfigureScan(config_a), AdapterError::Ok);
  EXPECT(controller_a.Arm(), AdapterError::Ok);
  EXPECT(controller_a.StartScan("job-a"), AdapterError::Ok);
  EXPECT(controller_a.StopScan(), AdapterError::Ok);
  tape_scanner::adapters::ControllerState stopped;
  EXPECT(controller_a.GetState(stopped), AdapterError::Ok);
  CHECK(stopped.motion == ControllerMotion::Stopped);
  CHECK(!stopped.ready);
  const double stopped_at = stopped.coordinates.carriage.accepted.value;
  EXPECT(controller_a.Move({"m4", Axis::Carriage, 3}), AdapterError::Stopped);
  EXPECT(controller_a.GetState(stopped), AdapterError::Ok);
  CHECK(Near(stopped.coordinates.carriage.accepted.value, stopped_at));
  EXPECT(controller_b.Move({"b2", Axis::Carriage, 21}), AdapterError::Ok);

  EXPECT(controller_a.AbortScan(), AdapterError::Fault);
  tape_scanner::adapters::ControllerState aborted;
  EXPECT(controller_a.GetState(aborted), AdapterError::Ok);
  CHECK(aborted.motion == ControllerMotion::Fault);
  CHECK(aborted.fault_code == "abort");
  CHECK(Near(aborted.coordinates.carriage.accepted.value, stopped_at));
  EXPECT(controller_a.ResetFault(), AdapterError::Ok);
  EXPECT(controller_a.GetState(aborted), AdapterError::Ok);
  CHECK(aborted.motion == ControllerMotion::Idle);
  CHECK(aborted.job_id.empty());
  EXPECT(controller_a.StartScan("job-a"), AdapterError::Rejected);

  controller_a.ExpireWatchdog();
  EXPECT(controller_a.Move({"m5", Axis::Carriage, 2}), AdapterError::Fault);
  EXPECT(controller_a.GetState(aborted), AdapterError::Ok);
  CHECK(aborted.fault_code == "watchdog");
  CHECK(Near(aborted.coordinates.carriage.accepted.value, stopped_at));
  EXPECT(controller_b.GetState(home_b), AdapterError::Ok);
  CHECK(home_b.motion != ControllerMotion::Fault);
  CHECK(Near(home_b.coordinates.carriage.accepted.value, 21));
  EXPECT(controller_a.ResetFault(), AdapterError::Ok);
  EXPECT(controller_a.StartScan("job-a"), AdapterError::Rejected);

  EXPECT(controller_a.SetLight(true), AdapterError::Ok);
  EXPECT(controller_a.GetState(aborted), AdapterError::Ok);
  CHECK(aborted.light_on.available);
  CHECK(aborted.light_on.value);

  EXPECT(camera_a.StartSession("scn-a", "ses-a"), AdapterError::Ok);
  EXPECT(ptp_b.StartSession("scn-b", "ses-b"), AdapterError::Ok);
  std::int64_t clock = 100;
  ptp_b.SetClock([&clock] { return clock; });
  Frame from_b;
  EXPECT(ptp_b.Capture({"scn-b", "ses-b", 2, 5, {}}, from_b), AdapterError::Ok);
  CHECK(from_b.scanner_id == "scn-b");
  CHECK(from_b.session_id == "ses-b");
  CHECK(from_b.format == PixelFormat::Mono8);
  CHECK(from_b.device_timestamp_ns.available);
  CHECK(from_b.device_timestamp_ns.value == 100);
  CHECK(from_b.buffer.size() == 8);
  CHECK(from_b.buffer.data()[0] == static_cast<std::uint8_t>(SimulatorPixel(2, 5, 0, 0, 8)));

  camera_a.SetLinkUp(false);
  Frame from_a;
  EXPECT(camera_a.Capture({"scn-a", "ses-a", 1, 1, {}}, from_a), AdapterError::LinkLost);
  CHECK(from_a.buffer.empty());
  CHECK(from_a.scanner_id == "scn-a");
  Frame again_b;
  EXPECT(ptp_b.Capture({"scn-b", "ses-b", 2, 6, {}}, again_b), AdapterError::Ok);
  CHECK(again_b.scanner_id == "scn-b");
  CHECK(again_b.buffer.size() == 8);
  CHECK(ptp_b.CompletedCaptures() == 2);
  CHECK(camera_a.CompletedCaptures() == 0);

  for (const auto& event : camera_a.Events()) {
    CHECK(event.scanner_id == "scn-a");
    CHECK(event.scanner_id.find("scn-b") == std::string::npos);
  }
  for (const auto& event : controller_b.Events()) {
    CHECK(event.scanner_id == "scn-b");
  }
  for (const auto& event : controller_a.Events()) {
    CHECK(event.scanner_id == "scn-a");
  }
}

void TestParallelIsolation() {
  CameraSimulator camera_a(SmallCamera("scn-a", "CAM-A", PixelFormat::Mono16, 2, 2));
  CameraSimulator camera_b(SmallCamera("scn-b", "CAM-B", PixelFormat::Mono8, 2, 2));
  CHECK(camera_a.Connect({"CAM-A", ""}));
  CHECK(camera_b.Connect({"CAM-B", ""}));
  CHECK(camera_a.StartSession("scn-a", "ses-a"));
  CHECK(camera_b.StartSession("scn-b", "ses-b"));

  constexpr int kFrames = 30;
  std::vector<Frame> frames_a(kFrames);
  std::vector<Frame> frames_b(kFrames);
  std::vector<AdapterError> errors_a(kFrames);
  std::vector<AdapterError> errors_b(kFrames);
  std::thread thread_a([&] {
    for (int i = 0; i < kFrames; ++i) {
      errors_a[i] =
          camera_a.Capture({"scn-a", "ses-a", 1, static_cast<std::uint64_t>(i), {}}, frames_a[i])
              .code;
    }
  });
  std::thread thread_b([&] {
    for (int i = 0; i < kFrames; ++i) {
      errors_b[i] =
          camera_b.Capture({"scn-b", "ses-b", 2, static_cast<std::uint64_t>(i), {}}, frames_b[i])
              .code;
    }
  });
  thread_a.join();
  thread_b.join();

  for (int i = 0; i < kFrames; ++i) {
    CHECK(errors_a[i] == AdapterError::Ok);
    CHECK(errors_b[i] == AdapterError::Ok);
    CHECK(frames_a[i].scanner_id == "scn-a");
    CHECK(frames_a[i].session_id == "ses-a");
    CHECK(frames_a[i].format == PixelFormat::Mono16);
    CHECK(ReadLe16(frames_a[i], 1, 1) == SimulatorPixel(1, static_cast<std::uint64_t>(i), 1, 1, 16));
    CHECK(frames_b[i].scanner_id == "scn-b");
    CHECK(frames_b[i].format == PixelFormat::Mono8);
    CHECK(frames_b[i].buffer.data()[0] ==
          static_cast<std::uint8_t>(SimulatorPixel(2, static_cast<std::uint64_t>(i), 0, 0, 8)));
  }
}

void TestEventLimit() {
  CameraSimulator camera(SmallCamera("scn-a", "CAM-A", PixelFormat::Mono8, 1, 1));
  CHECK(camera.Connect({"CAM-A", ""}));
  CHECK(camera.StartSession("scn-a", "ses-a"));
  for (int i = 0; i < 80; ++i) {
    Frame frame;
    CHECK(camera.Capture({"scn-a", "ses-a", 1, static_cast<std::uint64_t>(i), {}}, frame));
  }
  CHECK(camera.Events().size() == 64);
  CHECK(camera.EventsOverflowed());
  CHECK(camera.CompletedCaptures() == 80);
}

}  // namespace

int main() {
  TestContractHelpers();
  TestNominalFrame();
  TestFormatsAndFaultFrames();
  TestReferenceFrameSize();
  TestTwoScanners();
  TestParallelIsolation();
  TestEventLimit();
  if (g_failures != 0) {
    std::cerr << "test_adapters: " << g_failures << " failed out of " << g_checks << " checks\n";
    return 1;
  }
  std::cout << "test_adapters: " << g_checks << " checks passed\n";
  return 0;
}

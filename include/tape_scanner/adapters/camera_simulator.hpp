#pragma once

#include "tape_scanner/adapters/icamera.hpp"

#include <functional>
#include <mutex>
#include <vector>

namespace tape_scanner::adapters {

struct CameraProfile {
  std::string scanner_id;
  std::string model = "sim-gige";
  std::string serial;
  std::string firmware = "sim-1.0";
  std::string ip;
  int width = 1280;
  int height = 960;
  int max_width = 1280;
  int max_height = 960;
  PixelFormat format = PixelFormat::Mono16;
  std::vector<PixelFormat> formats{PixelFormat::Mono8, PixelFormat::Mono12, PixelFormat::Mono16};
  NumericRange exposure_us{16.0, 1'000'000.0, 1.0, true};
  NumericRange gain{0.0, 24.0, 0.1, true};
  double default_exposure_us = 1000.0;
  double default_gain = 0.0;
  bool ptp = false;
};

enum class CameraFaultKind { LinkLost, Timeout, Fault, Dropped, QueueOverflow };

inline CameraProfile MakeCameraProfile(std::string scanner_id, std::string serial) {
  CameraProfile profile;
  profile.scanner_id = std::move(scanner_id);
  profile.serial = std::move(serial);
  return profile;
}

// Внутрипроцессный симулятор камеры. Штатный кадр детерминирован.
// Аварийный сценарий задаётся очередью CameraFaultKind либо обрывом линии.
// Состояния другого экземпляра этот объект не видит.
class CameraSimulator final : public ICamera {
 public:
  explicit CameraSimulator(CameraProfile profile);
  ~CameraSimulator() override = default;

  CameraSimulator(const CameraSimulator&) = delete;
  CameraSimulator& operator=(const CameraSimulator&) = delete;

  [[nodiscard]] int InterfaceVersion() const override;

  AdapterResult Connect(const CameraEndpoint& endpoint) override;
  AdapterResult Disconnect() override;
  [[nodiscard]] bool IsConnected() const override;

  AdapterResult Identity(CameraIdentity& out) const override;
  AdapterResult Capabilities(CameraCapabilities& out) const override;

  AdapterResult SetExposureUs(double requested_us, double& actual_us) override;
  AdapterResult SetGain(double requested, double& actual) override;
  AdapterResult SetPixelFormat(PixelFormat format, PixelFormat& actual) override;
  AdapterResult SetFrameSize(int width, int height, FrameGeometry& actual) override;

  AdapterResult GetExposureUs(double& actual_us) const override;
  AdapterResult GetGain(double& actual) const override;
  AdapterResult GetPixelFormat(PixelFormat& actual) const override;

  AdapterResult StartSession(const std::string& scanner_id, const std::string& session_id) override;
  AdapterResult StopSession() override;
  AdapterResult Capture(const CaptureRequest& request, Frame& out) override;

  void SetOccupied(bool occupied);
  void SetLinkUp(bool up);
  void QueueFault(CameraFaultKind kind);
  void SetClock(std::function<std::int64_t()> clock);

  [[nodiscard]] AdapterResult CalibrationRelativePath(std::string& path) const;
  [[nodiscard]] std::vector<AdapterEvent> Events() const;
  [[nodiscard]] bool EventsOverflowed() const;
  [[nodiscard]] int CompletedCaptures() const;
  [[nodiscard]] const CameraProfile& Profile() const { return m_profile; }

 private:
  [[nodiscard]] AdapterResult RequireLiveLocked() const;
  [[nodiscard]] bool FormatSupportedLocked(PixelFormat format) const;
  void PushEventLocked(std::string name, AdapterError code);
  void ApplyNominalSettingsLocked();
  void FillExposureFieldsLocked(Frame& frame) const;
  [[nodiscard]] std::vector<std::uint8_t> RenderLocked(std::uint64_t tile_id,
                                                       std::uint64_t exposure_id) const;
  [[nodiscard]] std::int64_t NowLocked();

  CameraProfile m_profile;
  mutable std::mutex m_mutex;
  bool m_bound = false;
  bool m_link_up = true;
  bool m_occupied = false;
  std::string m_endpoint_ip;
  std::string m_session_id;
  PixelFormat m_format = PixelFormat::Mono16;
  int m_width = 0;
  int m_height = 0;
  Field<double> m_exposure_requested = UnavailableReal();
  Field<double> m_exposure_actual = UnavailableReal();
  Field<double> m_gain_requested = UnavailableReal();
  Field<double> m_gain_actual = UnavailableReal();
  std::vector<CameraFaultKind> m_faults;
  std::vector<AdapterEvent> m_events;
  bool m_events_overflow = false;
  int m_completed = 0;
  std::function<std::int64_t()> m_clock;
  std::int64_t m_clock_ns = 0;
};

}  // namespace tape_scanner::adapters

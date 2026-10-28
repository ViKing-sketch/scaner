#pragma once

#include "tape_scanner/adapters/icontroller.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace tape_scanner::adapters {

struct ControllerProfile {
  std::string scanner_id;
  std::string model = "sim-controller";
  std::string serial;
  std::string firmware = "sim-1.0";
  std::string protocol_version = "logical-1";
  double step = 1;
  double carriage_min = 0;
  double carriage_max = 100000;
  double film_min = 0;
  double film_max = 2000000;
  bool has_encoder = false;
  double encoder_bias = 0;
  bool report_film_sensor = true;
  bool film_present = false;
};

inline ControllerProfile MakeControllerProfile(std::string scanner_id, std::string serial) {
  ControllerProfile profile;
  profile.scanner_id = std::move(scanner_id);
  profile.serial = std::move(serial);
  return profile;
}

// Внутрипроцессный симулятор контроллера. Команды применяются сразу и
// возвращают логический результат. Потеря ответа помечает уже применённую
// команду, повтор того же command_id ось не сдвигает. TCP нет.
class ControllerSimulator final : public IController {
 public:
  explicit ControllerSimulator(ControllerProfile profile);
  ~ControllerSimulator() override = default;

  ControllerSimulator(const ControllerSimulator&) = delete;
  ControllerSimulator& operator=(const ControllerSimulator&) = delete;

  [[nodiscard]] int InterfaceVersion() const override;

  AdapterResult Connect(const ControllerEndpoint& endpoint) override;
  AdapterResult Disconnect() override;
  [[nodiscard]] bool IsConnected() const override;

  AdapterResult Identify(ControllerIdentity& out) const override;
  AdapterResult GetCapabilities(ControllerCapabilities& out) const override;
  AdapterResult GetState(ControllerState& out) const override;

  AdapterResult Home() override;
  AdapterResult Move(const MoveCommand& command) override;
  AdapterResult ConfigureScan(const ScanConfig& config) override;
  AdapterResult Arm() override;
  AdapterResult StartScan(const std::string& job_id) override;
  AdapterResult StopScan() override;
  AdapterResult AbortScan() override;
  AdapterResult ResetFault() override;
  AdapterResult Heartbeat() override;
  AdapterResult SetLight(bool on) override;

  void SetOccupied(bool occupied);
  void SetLinkUp(bool up);
  void ExpireWatchdog();
  void FailNextResponse();

  [[nodiscard]] AdapterResult CalibrationRelativePath(std::string& path) const;
  [[nodiscard]] std::vector<AdapterEvent> Events() const;
  [[nodiscard]] int AppliedMotionCount() const;
  [[nodiscard]] const ControllerProfile& Profile() const { return m_profile; }

 private:
  struct StoredConfig {
    bool present = false;
    ScanConfig config;
  };

  [[nodiscard]] AdapterResult RequireLiveLocked() const;
  [[nodiscard]] ControllerState BuildStateLocked() const;
  void PushEventLocked(std::string name, AdapterError code);
  void ClearJobLocked();
  void SetAxisLocked(AxisSample& axis, double commanded, double accepted) const;
  [[nodiscard]] AdapterResult ApplyPositionLocked(Axis axis, double commanded, double accepted);
  void LatchLimitLocked(Axis axis, bool positive);

  ControllerProfile m_profile;
  mutable std::mutex m_mutex;
  bool m_bound = false;
  bool m_link_up = true;
  bool m_occupied = false;
  bool m_referenced = false;
  bool m_armed = false;
  bool m_lose_next_response = false;
  ControllerMotion m_motion = ControllerMotion::Disconnected;
  std::string m_session_id;
  std::string m_job_id;
  std::string m_fault_code;
  std::string m_lock_source;
  std::string m_last_command_id;
  AxisSample m_carriage;
  AxisSample m_film;
  Field<bool> m_light = UnavailableField<bool>();
  Field<bool> m_film_present = UnavailableField<bool>();
  Field<bool> m_carriage_positive = UnavailableField<bool>();
  Field<bool> m_carriage_negative = UnavailableField<bool>();
  Field<bool> m_film_positive = UnavailableField<bool>();
  Field<bool> m_film_negative = UnavailableField<bool>();
  Field<double> m_speed = UnavailableReal();
  StoredConfig m_config;
  std::vector<AdapterEvent> m_events;
  bool m_events_overflow = false;
  int m_applied_motions = 0;
};

}  // namespace tape_scanner::adapters

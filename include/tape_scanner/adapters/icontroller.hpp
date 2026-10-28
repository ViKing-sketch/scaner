#pragma once

#include "tape_scanner/adapters/contract.hpp"

#include <string>

namespace tape_scanner::adapters {

enum class Axis { Carriage, Film };

enum class ControllerMotion {
  Disconnected,
  Idle,
  Armed,
  Scanning,
  Stopped,
  Fault,
  LinkLost
};

struct ControllerEndpoint {
  // Только идентичность. Сокет и разбор TCP здесь не открываются.
  std::string serial;
};

struct ControllerIdentity {
  std::string model;
  std::string serial;
  std::string firmware;
  // "logical-1" — логический контракт симулятора, не документ TCP-прошивки.
  std::string protocol_version;
  int interface_version = kAdapterInterfaceVersion;
};

struct ControllerCapabilities {
  bool carriage = false;
  bool film_axis = false;
  bool light = false;
  bool film_sensor = false;
  bool home = false;
  bool encoder = false;
  bool watchdog = false;
  bool watchdog_timeout_known = false;
  bool xray_control = false;
  bool tcp_transport = false;
  double step = 0;
  double carriage_min = 0;
  double carriage_max = 0;
  double film_min = 0;
  double film_max = 0;
};

struct ControllerState {
  int interface_version = kAdapterInterfaceVersion;
  std::string scanner_id;
  std::string session_id;
  std::string job_id;
  ControllerMotion motion = ControllerMotion::Disconnected;
  MechanicsCoordinates coordinates;
  Field<bool> film_present;
  Field<bool> light_on;
  Field<bool> carriage_positive_limit;
  Field<bool> carriage_negative_limit;
  Field<bool> film_positive_limit;
  Field<bool> film_negative_limit;
  // Неподключённый аварийный вход не выдаётся как «ложь».
  Field<bool> emergency_stop;
  Field<double> speed = UnavailableReal();
  bool ready = false;
  bool referenced = false;
  std::string fault_code;
  std::string lock_source;
};

struct MoveCommand {
  std::string command_id;
  Axis axis = Axis::Carriage;
  double commanded = 0;
};

struct ScanConfig {
  std::string session_id;
  std::string job_id;
  double carriage_start = 0;
  double carriage_end = 0;
  double film_start = 0;
  double film_end = 0;
  double speed = 0;
};

// Логический контракт контроллера. Повтор command_id не двигает ось ещё раз.
// Потеря ответа не является разрешением повторить движение: положение читается
// через GetState. Кадрирование TCP сюда не входит.
class IController {
 public:
  virtual ~IController() = default;

  [[nodiscard]] virtual int InterfaceVersion() const = 0;

  virtual AdapterResult Connect(const ControllerEndpoint& endpoint) = 0;
  virtual AdapterResult Disconnect() = 0;
  [[nodiscard]] virtual bool IsConnected() const = 0;

  virtual AdapterResult Identify(ControllerIdentity& out) const = 0;
  virtual AdapterResult GetCapabilities(ControllerCapabilities& out) const = 0;
  virtual AdapterResult GetState(ControllerState& out) const = 0;

  virtual AdapterResult Home() = 0;
  virtual AdapterResult Move(const MoveCommand& command) = 0;
  virtual AdapterResult ConfigureScan(const ScanConfig& config) = 0;
  virtual AdapterResult Arm() = 0;
  virtual AdapterResult StartScan(const std::string& job_id) = 0;
  virtual AdapterResult StopScan() = 0;
  virtual AdapterResult AbortScan() = 0;
  virtual AdapterResult ResetFault() = 0;
  virtual AdapterResult Heartbeat() = 0;
  virtual AdapterResult SetLight(bool on) = 0;
};

}  // namespace tape_scanner::adapters

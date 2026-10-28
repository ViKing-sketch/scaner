#pragma once

#include "tape_scanner/adapters/contract.hpp"

#include <string>
#include <vector>

namespace tape_scanner::adapters {

struct CameraEndpoint {
  std::string serial;
  // Пустая строка не запускает поиск. Если профиль задаёт IP, он сравнивается
  // как идентификатор, без открытия сокета и без перебора dev_list[0].
  std::string ip;
};

struct CameraIdentity {
  std::string model;
  std::string serial;
  std::string firmware;
  std::string endpoint_ip;
  std::string adapter_name;
  std::string adapter_version;
  int interface_version = kAdapterInterfaceVersion;
  bool genicam_xml_available = false;
  std::string genicam_xml;
};

struct CameraCapabilities {
  std::vector<PixelFormat> pixel_formats;
  NumericRange exposure_us;
  NumericRange gain;
  int max_width = 0;
  int max_height = 0;
  int width = 0;
  int height = 0;
  bool exposure_writable = false;
  bool gain_writable = false;
  bool ptp = false;
  bool action_commands = false;
  bool chunk_data = false;
  bool hardware_trigger = false;
  bool encoder_counter = false;
  // Симулятор не встраивает транспорт SDK и не пишет свой GigE-стек.
  bool uses_vendor_sdk = false;
  bool uses_gentl = false;
};

struct FrameGeometry {
  int width = 0;
  int height = 0;
};

struct CaptureRequest {
  std::string scanner_id;
  std::string session_id;
  std::uint64_t tile_id = 0;
  std::uint64_t exposure_id = 0;
  // Координаты приходят от контроллера. Камера их не выдумывает и не заменяет
  // недоступное измерение нулём.
  MechanicsCoordinates coordinates;
};

// Версия интерфейса — kAdapterInterfaceVersion. Реализация без wxWidgets и SDK.
class ICamera {
 public:
  virtual ~ICamera() = default;

  [[nodiscard]] virtual int InterfaceVersion() const = 0;

  virtual AdapterResult Connect(const CameraEndpoint& endpoint) = 0;
  virtual AdapterResult Disconnect() = 0;
  [[nodiscard]] virtual bool IsConnected() const = 0;

  virtual AdapterResult Identity(CameraIdentity& out) const = 0;
  virtual AdapterResult Capabilities(CameraCapabilities& out) const = 0;

  virtual AdapterResult SetExposureUs(double requested_us, double& actual_us) = 0;
  virtual AdapterResult SetGain(double requested, double& actual) = 0;
  virtual AdapterResult SetPixelFormat(PixelFormat format, PixelFormat& actual) = 0;
  virtual AdapterResult SetFrameSize(int width, int height, FrameGeometry& actual) = 0;

  virtual AdapterResult GetExposureUs(double& actual_us) const = 0;
  virtual AdapterResult GetGain(double& actual) const = 0;
  virtual AdapterResult GetPixelFormat(PixelFormat& actual) const = 0;

  virtual AdapterResult StartSession(const std::string& scanner_id,
                                     const std::string& session_id) = 0;
  virtual AdapterResult StopSession() = 0;
  virtual AdapterResult Capture(const CaptureRequest& request, Frame& out) = 0;
};

}  // namespace tape_scanner::adapters

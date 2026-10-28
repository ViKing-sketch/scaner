#pragma once

// Версионируемый контракт адаптеров (ARC-02, ARC-04, SCL-01).
// Публичные типы не содержат wxWindow, дескриптор SDK и сокет.
// Транспорт GigE/TCP сюда не входит: SDK остаётся за адаптером камеры,
// разбор TCP-протокола контроллера — отдельная задача.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tape_scanner::adapters {

inline constexpr int kAdapterInterfaceVersion = 1;

inline constexpr std::uint64_t kSimulatorPixelExposure = 131;
inline constexpr std::uint64_t kSimulatorPixelTile = 17;
inline constexpr std::uint64_t kSimulatorPixelColumn = 3;

enum class AdapterError {
  Ok = 0,
  NotConnected,
  Busy,
  WrongDevice,
  LinkLost,
  Timeout,
  IncompatibleFormat,
  ParameterRejected,
  Fault,
  Rejected,
  Stopped,
  Dropped,
  QueueOverflow,
  Unavailable
};

struct AdapterResult {
  AdapterError code = AdapterError::Ok;
  std::string message;

  [[nodiscard]] bool ok() const { return code == AdapterError::Ok; }
  explicit operator bool() const { return ok(); }

  static AdapterResult Ok() { return {}; }

  static AdapterResult Fail(AdapterError error, std::string text) {
    AdapterResult result;
    result.code = error;
    result.message = std::move(text);
    return result;
  }
};

// Происхождение поля. Недоступное аппаратное значение нельзя подменять нулём:
// available == false и origin == Unavailable. Для вещественных величин value
// при этом NaN, чтобы случайное чтение не выглядело как измеренный ноль.
enum class FieldOrigin {
  Unavailable = 0,
  Commanded,
  Accepted,
  Measured,
  Device,
  Host
};

template <typename T>
struct Field {
  T value{};
  FieldOrigin origin = FieldOrigin::Unavailable;
  bool available = false;
};

template <typename T>
Field<T> MakeField(T value, FieldOrigin origin) {
  Field<T> field;
  field.value = std::move(value);
  field.origin = origin;
  field.available = true;
  return field;
}

template <typename T>
Field<T> UnavailableField() {
  return {};
}

inline Field<double> UnavailableReal() {
  Field<double> field;
  field.value = std::numeric_limits<double>::quiet_NaN();
  field.origin = FieldOrigin::Unavailable;
  field.available = false;
  return field;
}

inline bool IsUnavailableReal(const Field<double>& field) {
  return !field.available && field.origin == FieldOrigin::Unavailable && std::isnan(field.value);
}

enum class PixelFormat {
  Unknown = 0,
  Mono8,
  Mono12,
  Mono12Packed,
  Mono12p,
  Mono16
};

enum class AcquisitionStatus {
  None = 0,
  Ok,
  Dropped,
  Timeout,
  LinkLost,
  QueueOverflow,
  Fault
};

struct NumericRange {
  double minimum = 0;
  double maximum = 0;
  double step = 1;
  bool available = false;
};

struct AxisSample {
  Field<double> commanded = UnavailableReal();
  Field<double> accepted = UnavailableReal();
  Field<double> measured = UnavailableReal();
  // Единица системы координат профиля, а не признак измеренности.
  std::string unit = "step";
};

struct MechanicsCoordinates {
  AxisSample carriage;
  AxisSample film;
};

// Неизменяемый буфер кадра.
// Адаптер SDK обязан скопировать байты в SealBuffer и только после этого
// освободить буфер SDK (RAII на всех выходах). Кадр не хранит указатель SDK.
// Потребитель может удерживать shared_ptr, но не меняет байты.
struct FrameBuffer {
  std::shared_ptr<const std::vector<std::uint8_t>> bytes;

  [[nodiscard]] bool empty() const { return bytes == nullptr || bytes->empty(); }
  [[nodiscard]] const std::uint8_t* data() const {
    return empty() ? nullptr : bytes->data();
  }
  [[nodiscard]] std::size_t size() const { return bytes == nullptr ? 0 : bytes->size(); }
};

inline FrameBuffer SealBuffer(std::vector<std::uint8_t> bytes) {
  FrameBuffer buffer;
  buffer.bytes = std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));
  return buffer;
}

struct Frame {
  int interface_version = kAdapterInterfaceVersion;
  std::string scanner_id;
  std::string session_id;
  std::uint64_t tile_id = 0;
  std::uint64_t exposure_id = 0;
  int width = 0;
  int height = 0;
  int stride = 0;
  PixelFormat format = PixelFormat::Unknown;
  int significant_bits = 0;
  Field<double> exposure_requested_us = UnavailableReal();
  Field<double> exposure_us = UnavailableReal();
  Field<double> gain_requested = UnavailableReal();
  Field<double> gain = UnavailableReal();
  Field<std::int64_t> host_timestamp_ns = UnavailableField<std::int64_t>();
  Field<std::uint64_t> device_timestamp_ns = UnavailableField<std::uint64_t>();
  MechanicsCoordinates coordinates;
  AcquisitionStatus status = AcquisitionStatus::None;
  AdapterError error = AdapterError::Ok;
  std::string status_detail;
  FrameBuffer buffer;
};

struct AdapterEvent {
  std::string scanner_id;
  std::string session_id;
  std::string name;
  AdapterError code = AdapterError::Ok;
};

inline bool IsDeviceId(std::string_view id) {
  if (id.empty() || id.size() > 64) {
    return false;
  }
  for (unsigned char c : id) {
    const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z') || c == '-' || c == '_';
    if (!ok) {
      return false;
    }
  }
  return true;
}

// Калибровка принадлежит паре scanner_id/session_id.
// Глобальное имя coefs.xml для разных устройств запрещено.
inline AdapterResult CalibrationPath(std::string_view scanner_id, std::string_view session_id,
                                     std::string& path) {
  if (!IsDeviceId(scanner_id) || !IsDeviceId(session_id)) {
    path.clear();
    return AdapterResult::Fail(AdapterError::ParameterRejected,
                               "calibration requires scanner_id and session_id");
  }
  path = "calibrations/";
  path.append(scanner_id);
  path.push_back('/');
  path.append(session_id);
  path.append("/coefs.xml");
  return AdapterResult::Ok();
}

inline bool IsGlobalCoefsName(std::string_view path) {
  return path == "coefs.xml" || path == "calibrations/coefs.xml" || path == "/coefs.xml";
}

inline int SignificantBits(PixelFormat format) {
  switch (format) {
  case PixelFormat::Mono8:
    return 8;
  case PixelFormat::Mono12:
  case PixelFormat::Mono12Packed:
  case PixelFormat::Mono12p:
    return 12;
  case PixelFormat::Mono16:
    return 16;
  case PixelFormat::Unknown:
    return 0;
  }
  return 0;
}

// Плотный контейнер, который симулятор реально заполняет.
// Mono12Packed и Mono12p здесь не раскладываются: их декодер — отдельная задача.
inline int UnpackedBytesPerPixel(PixelFormat format) {
  switch (format) {
  case PixelFormat::Mono8:
    return 1;
  case PixelFormat::Mono12:
  case PixelFormat::Mono16:
    return 2;
  case PixelFormat::Mono12Packed:
  case PixelFormat::Mono12p:
  case PixelFormat::Unknown:
    return 0;
  }
  return 0;
}

inline bool FrameByteSize(int stride, int height, std::size_t& bytes) {
  if (stride <= 0 || height <= 0) {
    return false;
  }
  const auto stride_u = static_cast<std::size_t>(stride);
  const auto height_u = static_cast<std::size_t>(height);
  if (stride_u > std::numeric_limits<std::size_t>::max() / height_u) {
    return false;
  }
  bytes = stride_u * height_u;
  return true;
}

inline AdapterResult QuantizeRange(const NumericRange& range, double requested, double& actual) {
  if (!range.available || !(range.step > 0.0) || !(range.minimum <= range.maximum)) {
    return AdapterResult::Fail(AdapterError::Unavailable, "parameter range is unavailable");
  }
  const double epsilon = std::max(1e-9, std::abs(range.step) * 1e-6);
  if (requested < range.minimum - epsilon || requested > range.maximum + epsilon) {
    return AdapterResult::Fail(AdapterError::ParameterRejected, "value is outside the device range");
  }
  const long long steps = std::llround((requested - range.minimum) / range.step);
  double quantized = range.minimum + static_cast<double>(steps) * range.step;
  if (quantized < range.minimum) {
    quantized = range.minimum;
  }
  if (quantized > range.maximum) {
    quantized = range.maximum;
  }
  actual = quantized;
  return AdapterResult::Ok();
}

inline std::uint32_t SimulatorPixel(std::uint64_t tile_id, std::uint64_t exposure_id, int x, int y,
                                    int significant_bits) {
  const auto mixed = exposure_id * kSimulatorPixelExposure + tile_id * kSimulatorPixelTile +
                     static_cast<std::uint64_t>(x) * kSimulatorPixelColumn +
                     static_cast<std::uint64_t>(y);
  if (significant_bits <= 0 || significant_bits > 16) {
    return 0;
  }
  const std::uint32_t mask =
      significant_bits == 16 ? 0xffffu : ((1u << static_cast<unsigned>(significant_bits)) - 1u);
  return static_cast<std::uint32_t>(mixed & mask);
}

inline Frame ClearedFrame() {
  Frame frame;
  frame.interface_version = kAdapterInterfaceVersion;
  return frame;
}

}  // namespace tape_scanner::adapters

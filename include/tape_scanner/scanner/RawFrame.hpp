#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/**
 * Распаковка RAW без SDK и без wxWidgets.
 *
 * Декодер выбирается по коду uiMediaType из заголовка кадра и по длине буфера.
 * CameraSetIspOutFormat этот код не заменяет. Отсчёты не сдвигаются на 18 и не
 * растягиваются до 65535: в samples лежат исходные коды, significant_bits — сколько
 * из них значащие, stored_bits — разрядность контейнера uint16 (всегда 16).
 *
 * Числовые коды MONO8 / MONO12 / MONO12_PACKED / MONO16 / MONO12_PACKED_MV совпадают
 * с макросами CameraDefine.h этого репозитория. kMediaMono12p в том заголовке нет.
 *
 * Упаковка (допущение, пока паспорт камеры не назвал фактический RAW):
 * - MONO12_PACKED — GigE Vision Mono12Packed: младший полубайт среднего байта
 *   принадлежит первому пикселю пары;
 * - MONO12_PACKED_MV — прежний цикл Camera.cpp: старший полубайт среднего байта
 *   принадлежит первому пикселю пары;
 * - kMediaMono12p — PFNC Mono12p, младшие биты пикселя идут первыми.
 * Нечётная ширина: пары по 3 байта, последний пиксель строки — 2 байта, неиспользованный
 * полубайт обязан быть нулём. Строки не склеиваются в общий битовый поток.
 * Mono12/Mono16 — uint16, по умолчанию little-endian; старшие 4 бита Mono12 обязаны быть 0.
 * stride == 0 выводится как size/height, если это целое число байт и его хватает на строку.
 * Хвост строки короче stride — транспортный padding, не пиксели.
 */

namespace tape_scanner {

inline constexpr uint32_t kMediaMono = 0x01000000u;
inline constexpr uint32_t kMediaOccupy8Bit = 0x00080000u;
inline constexpr uint32_t kMediaOccupy12Bit = 0x000C0000u;
inline constexpr uint32_t kMediaOccupy16Bit = 0x00100000u;

inline constexpr uint32_t kMediaMono8 = kMediaMono | kMediaOccupy8Bit | 0x0001u;
inline constexpr uint32_t kMediaMono12 = kMediaMono | kMediaOccupy16Bit | 0x0005u;
inline constexpr uint32_t kMediaMono12Packed = kMediaMono | kMediaOccupy12Bit | 0x0006u;
inline constexpr uint32_t kMediaMono16 = kMediaMono | kMediaOccupy16Bit | 0x0007u;
inline constexpr uint32_t kMediaMono12PackedMv = kMediaMono | kMediaOccupy12Bit | 0x0064u;
/// Не код SDK. Явная метка PFNC Mono12p, пока в CameraDefine.h нет своего константного имени.
inline constexpr uint32_t kMediaMono12p = 0x010C00A0u;

/// Предел декодера, не измеренный размер сенсора.
inline constexpr int kMaxRawDimension = 32000000;
inline constexpr std::size_t kMaxRawSamples = 32000000;

enum class RawByteOrder { LittleEndian, BigEndian };

enum class RawDecodeStatus {
  Ok,
  NullBuffer,
  UnsupportedFormat,
  InvalidGeometry,
  WrongLength,
  InvalidPacking,
  UnsupportedByteOrder,
};

struct RawDecodeRequest {
  uint32_t media_type = 0;
  int width = 0;
  int height = 0;
  /// 0 — вывести stride из size и height. Иначе точный шаг строки в байтах.
  int stride = 0;
  RawByteOrder byte_order = RawByteOrder::LittleEndian;
  const uint8_t *data = nullptr;
  std::size_t size = 0;
};

struct RawImage {
  uint32_t media_type = 0;
  int width = 0;
  int height = 0;
  int stride = 0;
  int significant_bits = 0;
  int stored_bits = 0;
  RawByteOrder byte_order = RawByteOrder::LittleEndian;
  std::vector<uint16_t> samples;
};

struct RawDecodeResult {
  RawDecodeStatus status = RawDecodeStatus::UnsupportedFormat;
  RawImage image;
};

bool RawFormatSupported(uint32_t media_type);
int RawSignificantBits(uint32_t media_type);
const char *RawDecodeStatusText(RawDecodeStatus status);
RawDecodeResult DecodeRawFrame(const RawDecodeRequest &request);

} // namespace tape_scanner

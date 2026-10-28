#include <tape_scanner/scanner/RawFrame.hpp>

#include <opencv2/core/version.hpp>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const char *expr, int line) {
  ++g_checks;
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

using tape_scanner::DecodeRawFrame;
using tape_scanner::RawByteOrder;
using tape_scanner::RawDecodeRequest;
using tape_scanner::RawDecodeResult;
using tape_scanner::RawDecodeStatus;
using tape_scanner::kMediaMono12;
using tape_scanner::kMediaMono12Packed;
using tape_scanner::kMediaMono12PackedMv;
using tape_scanner::kMediaMono12p;
using tape_scanner::kMediaMono16;
using tape_scanner::kMediaMono8;

void AppendU16(std::vector<uint8_t> &out, uint16_t value, RawByteOrder order) {
  if (order == RawByteOrder::LittleEndian) {
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
    out.push_back(static_cast<uint8_t>(value >> 8));
  } else {
    out.push_back(static_cast<uint8_t>(value >> 8));
    out.push_back(static_cast<uint8_t>(value & 0xFFu));
  }
}

std::vector<uint8_t> EncodeRow(uint32_t media, RawByteOrder order, int width, const uint16_t *pixels) {
  std::vector<uint8_t> row;
  if (media == kMediaMono8) {
    for (int x = 0; x < width; ++x) {
      row.push_back(static_cast<uint8_t>(pixels[x]));
    }
    return row;
  }
  if (media == kMediaMono12 || media == kMediaMono16) {
    for (int x = 0; x < width; ++x) {
      AppendU16(row, pixels[x], order);
    }
    return row;
  }

  int x = 0;
  for (; x + 1 < width; x += 2) {
    const uint16_t p0 = pixels[x];
    const uint16_t p1 = pixels[x + 1];
    if (media == kMediaMono12Packed) {
      row.push_back(static_cast<uint8_t>(p0 >> 4));
      row.push_back(static_cast<uint8_t>(((p1 & 0x0Fu) << 4) | (p0 & 0x0Fu)));
      row.push_back(static_cast<uint8_t>(p1 >> 4));
    } else if (media == kMediaMono12PackedMv) {
      row.push_back(static_cast<uint8_t>(p0 >> 4));
      row.push_back(static_cast<uint8_t>(((p0 & 0x0Fu) << 4) | (p1 & 0x0Fu)));
      row.push_back(static_cast<uint8_t>(p1 >> 4));
    } else {
      row.push_back(static_cast<uint8_t>(p0 & 0xFFu));
      row.push_back(static_cast<uint8_t>(((p1 & 0x0Fu) << 4) | ((p0 >> 8) & 0x0Fu)));
      row.push_back(static_cast<uint8_t>(p1 >> 4));
    }
  }
  if (x < width) {
    const uint16_t p0 = pixels[x];
    if (media == kMediaMono12Packed) {
      row.push_back(static_cast<uint8_t>(p0 >> 4));
      row.push_back(static_cast<uint8_t>(p0 & 0x0Fu));
    } else if (media == kMediaMono12PackedMv) {
      row.push_back(static_cast<uint8_t>(p0 >> 4));
      row.push_back(static_cast<uint8_t>((p0 & 0x0Fu) << 4));
    } else {
      row.push_back(static_cast<uint8_t>(p0 & 0xFFu));
      row.push_back(static_cast<uint8_t>((p0 >> 8) & 0x0Fu));
    }
  }
  return row;
}

std::vector<uint8_t> Encode(uint32_t media, RawByteOrder order, int width, int height, const std::vector<uint16_t> &pixels,
                            int pad) {
  std::vector<uint8_t> frame;
  for (int y = 0; y < height; ++y) {
    auto row = EncodeRow(media, order, width, pixels.data() + static_cast<std::size_t>(y) * width);
    frame.insert(frame.end(), row.begin(), row.end());
    frame.insert(frame.end(), static_cast<std::size_t>(pad), 0xA5);
  }
  return frame;
}

uint16_t Gradient(int x, int y, int width, int significant_bits) {
  const uint16_t mask = significant_bits >= 16 ? 0xFFFFu : static_cast<uint16_t>((1u << significant_bits) - 1u);
  if (x == 0 && y == 0) {
    return 0;
  }
  if (x == width - 1 && y == 0) {
    return mask;
  }
  if (x == 1 && y == 0) {
    return 1;
  }
  if (x == 2 && y == 0 && mask >= 17) {
    return 17;
  }
  if (x == 3 && y == 0 && mask >= 18) {
    return 18;
  }
  if (x == 4 && y == 0 && mask >= 256) {
    return 256;
  }
  if (x == 5 && y == 0 && mask >= 257) {
    return 257;
  }
  return static_cast<uint16_t>((x * 17 + y * 29) & mask);
}

RawDecodeResult Decode(uint32_t media, RawByteOrder order, int width, int height, const uint8_t *data, std::size_t size, int stride) {
  RawDecodeRequest request;
  request.media_type = media;
  request.width = width;
  request.height = height;
  request.stride = stride;
  request.byte_order = order;
  request.data = data;
  request.size = size;
  return DecodeRawFrame(request);
}

RawDecodeResult Decode(uint32_t media, RawByteOrder order, int width, int height, const std::vector<uint8_t> &bytes, int stride) {
  return Decode(media, order, width, height, bytes.data(), bytes.size(), stride);
}

void ExpectRoundTrip(uint32_t media, RawByteOrder order, int width, int height, int pad, int significant_bits) {
  std::vector<uint16_t> pixels(static_cast<std::size_t>(width) * height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      pixels[static_cast<std::size_t>(y) * width + x] = Gradient(x, y, width, significant_bits);
    }
  }
  const auto bytes = Encode(media, order, width, height, pixels, pad);
  const int tight = static_cast<int>(EncodeRow(media, order, width, pixels.data()).size());
  const auto inferred = Decode(media, order, width, height, bytes, 0);
  CHECK(inferred.status == RawDecodeStatus::Ok);
  CHECK(inferred.image.samples == pixels);
  CHECK(inferred.image.significant_bits == significant_bits);
  CHECK(inferred.image.stored_bits == 16);
  CHECK(inferred.image.media_type == media);
  CHECK(inferred.image.stride == tight + pad);
  CHECK(inferred.image.width == width);
  CHECK(inferred.image.height == height);

  const auto explicit_stride = Decode(media, order, width, height, bytes, tight + pad);
  CHECK(explicit_stride.status == RawDecodeStatus::Ok);
  CHECK(explicit_stride.image.samples == pixels);
  CHECK(explicit_stride.image.samples.front() == pixels.front());
  CHECK(explicit_stride.image.samples.back() == pixels.back());
  CHECK(explicit_stride.image.samples[static_cast<std::size_t>(width - 1)] == pixels[static_cast<std::size_t>(width - 1)]);
  if (height > 1) {
    CHECK(explicit_stride.image.samples[static_cast<std::size_t>(width)] == pixels[static_cast<std::size_t>(width)]);
  }
}

void ExpectStatus(const RawDecodeResult &result, RawDecodeStatus status) {
  if (result.status != status) {
    std::cerr << "status " << tape_scanner::RawDecodeStatusText(result.status) << " expected "
              << tape_scanner::RawDecodeStatusText(status) << "\n";
  }
  CHECK(result.status == status);
  CHECK(result.image.samples.empty());
}

} // namespace

int main() {
  std::cout << "OpenCV " << CV_VERSION << "\n";
  CHECK(std::string(CV_VERSION).size() > 0);

  CHECK(tape_scanner::kMediaMono8 == 0x01080001u);
  CHECK(tape_scanner::kMediaMono12 == 0x01100005u);
  CHECK(tape_scanner::kMediaMono12Packed == 0x010C0006u);
  CHECK(tape_scanner::kMediaMono16 == 0x01100007u);
  CHECK(tape_scanner::kMediaMono12PackedMv == 0x010C0064u);
  CHECK(tape_scanner::kMediaMono12p == 0x010C00A0u);
  CHECK(tape_scanner::kMediaMono12p != tape_scanner::kMediaMono12Packed);
  CHECK(tape_scanner::kMediaMono12Packed != tape_scanner::kMediaMono12PackedMv);
  CHECK(tape_scanner::RawSignificantBits(kMediaMono8) == 8);
  CHECK(tape_scanner::RawSignificantBits(kMediaMono12) == 12);
  CHECK(tape_scanner::RawSignificantBits(kMediaMono12Packed) == 12);
  CHECK(tape_scanner::RawSignificantBits(kMediaMono12PackedMv) == 12);
  CHECK(tape_scanner::RawSignificantBits(kMediaMono12p) == 12);
  CHECK(tape_scanner::RawSignificantBits(kMediaMono16) == 16);
  CHECK(tape_scanner::RawSignificantBits(0) == 0);
  CHECK(!tape_scanner::RawFormatSupported(0x01100003u));

  const uint32_t formats[] = {kMediaMono8, kMediaMono12, kMediaMono16, kMediaMono12Packed, kMediaMono12PackedMv, kMediaMono12p};
  const int bits[] = {8, 12, 16, 12, 12, 12};
  for (int i = 0; i < 6; ++i) {
    const RawByteOrder orders[] = {RawByteOrder::LittleEndian, RawByteOrder::BigEndian};
    const int order_count = (formats[i] == kMediaMono12 || formats[i] == kMediaMono16) ? 2 : 1;
    for (int order_i = 0; order_i < order_count; ++order_i) {
      ExpectRoundTrip(formats[i], orders[order_i], 1, 1, 0, bits[i]);
      ExpectRoundTrip(formats[i], orders[order_i], 2, 1, 0, bits[i]);
      ExpectRoundTrip(formats[i], orders[order_i], 3, 2, 0, bits[i]);
      ExpectRoundTrip(formats[i], orders[order_i], 7, 3, 5, bits[i]);
      ExpectRoundTrip(formats[i], orders[order_i], 8, 2, 1, bits[i]);
    }
  }

  // Одинаковые три байта не являются одним форматом.
  const uint8_t shared[] = {0xAB, 0x3C, 0x12};
  const auto gige = Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 2, 1, shared, 3, 0);
  const auto mv = Decode(kMediaMono12PackedMv, RawByteOrder::LittleEndian, 2, 1, shared, 3, 0);
  const auto p12 = Decode(kMediaMono12p, RawByteOrder::LittleEndian, 2, 1, shared, 3, 0);
  CHECK(gige.status == RawDecodeStatus::Ok);
  CHECK(mv.status == RawDecodeStatus::Ok);
  CHECK(p12.status == RawDecodeStatus::Ok);
  CHECK(gige.image.samples.size() == 2);
  CHECK(gige.image.samples[0] == 0x0ABC);
  CHECK(gige.image.samples[1] == 0x0123);
  CHECK(mv.image.samples[0] == 0x0AB3);
  CHECK(mv.image.samples[1] == 0x012C);
  CHECK(p12.image.samples[0] == 0x0CAB);
  CHECK(p12.image.samples[1] == 0x0123);
  CHECK(gige.image.samples != mv.image.samples);
  CHECK(gige.image.samples != p12.image.samples);
  CHECK(mv.image.samples != p12.image.samples);

  // Mono12 и Mono16 с нулевыми старшими битами дают тот же код, но не ту же разрядность.
  const uint8_t low12[] = {0xFF, 0x0F};
  const auto as12 = Decode(kMediaMono12, RawByteOrder::LittleEndian, 1, 1, low12, 2, 0);
  const auto as16 = Decode(kMediaMono16, RawByteOrder::LittleEndian, 1, 1, low12, 2, 0);
  CHECK(as12.status == RawDecodeStatus::Ok);
  CHECK(as16.status == RawDecodeStatus::Ok);
  CHECK(as12.image.samples[0] == 0x0FFF);
  CHECK(as16.image.samples[0] == 0x0FFF);
  CHECK(as12.image.significant_bits == 12);
  CHECK(as16.image.significant_bits == 16);

  const uint8_t high[] = {0xFF, 0x1F};
  ExpectStatus(Decode(kMediaMono12, RawByteOrder::LittleEndian, 1, 1, high, 2, 0), RawDecodeStatus::InvalidPacking);
  const auto high16 = Decode(kMediaMono16, RawByteOrder::LittleEndian, 1, 1, high, 2, 0);
  CHECK(high16.status == RawDecodeStatus::Ok);
  CHECK(high16.image.samples[0] == 0x1FFF);

  const uint8_t be[] = {0x12, 0x34};
  const auto be16 = Decode(kMediaMono16, RawByteOrder::BigEndian, 1, 1, be, 2, 0);
  const auto le16 = Decode(kMediaMono16, RawByteOrder::LittleEndian, 1, 1, be, 2, 0);
  CHECK(be16.status == RawDecodeStatus::Ok);
  CHECK(be16.image.samples[0] == 0x1234);
  CHECK(le16.image.samples[0] == 0x3412);

  ExpectStatus(Decode(kMediaMono12Packed, RawByteOrder::BigEndian, 2, 1, shared, 3, 0), RawDecodeStatus::UnsupportedByteOrder);
  const uint8_t mono8[] = {0x00, 0xFF};
  ExpectStatus(Decode(kMediaMono8, RawByteOrder::BigEndian, 2, 1, mono8, 2, 0), RawDecodeStatus::UnsupportedByteOrder);

  // Насыщение остаётся кодом сенсора: без вычитания 18 и без растяжения 65536/3800.
  const uint16_t saturated[] = {0, 17, 4095};
  const auto sat_bytes = Encode(kMediaMono12Packed, RawByteOrder::LittleEndian, 3, 1, {saturated, saturated + 3}, 0);
  const auto sat = Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 3, 1, sat_bytes, 0);
  CHECK(sat.status == RawDecodeStatus::Ok);
  CHECK(sat.image.samples[0] == 0);
  CHECK(sat.image.samples[1] == 17);
  CHECK(sat.image.samples[2] == 4095);
  CHECK(sat.image.samples[1] != 0);
  CHECK(sat.image.samples[2] != 65535);
  const uint8_t full16[] = {0xFF, 0xFF, 0x00, 0x00};
  const auto sat16 = Decode(kMediaMono16, RawByteOrder::LittleEndian, 2, 1, full16, 4, 0);
  CHECK(sat16.status == RawDecodeStatus::Ok);
  CHECK(sat16.image.samples[0] == 65535);
  CHECK(sat16.image.samples[1] == 0);
  CHECK(sat16.image.stored_bits == 16);
  CHECK(sat.image.samples[1] != sat.image.samples[2]);

  // Грязный неиспользованный полубайт нечётного хвоста и короткая длина.
  const uint8_t dirty_tail[] = {0x10, 0xF1};
  ExpectStatus(Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 1, 1, dirty_tail, 2, 0), RawDecodeStatus::InvalidPacking);
  const uint8_t dirty_mv[] = {0x10, 0x1F};
  ExpectStatus(Decode(kMediaMono12PackedMv, RawByteOrder::LittleEndian, 1, 1, dirty_mv, 2, 0), RawDecodeStatus::InvalidPacking);
  const uint8_t dirty_p[] = {0xBC, 0xFA};
  ExpectStatus(Decode(kMediaMono12p, RawByteOrder::LittleEndian, 1, 1, dirty_p, 2, 0), RawDecodeStatus::InvalidPacking);

  const uint16_t pair[] = {1, 2, 3, 4};
  const auto even_only = Encode(kMediaMono12Packed, RawByteOrder::LittleEndian, 2, 1, {pair, pair + 2}, 0);
  ExpectStatus(Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 3, 1, even_only, 0), RawDecodeStatus::WrongLength);
  ExpectStatus(Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 2, 1, even_only.data(), even_only.size() - 1, 0),
               RawDecodeStatus::WrongLength);
  // Один лишний байт одной строки — padding, а не ещё пиксель.
  auto padded_tail = even_only;
  padded_tail.push_back(0x5A);
  const auto padded_ok = Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 2, 1, padded_tail, 0);
  CHECK(padded_ok.status == RawDecodeStatus::Ok);
  CHECK(padded_ok.image.samples.size() == 2);
  CHECK(padded_ok.image.samples[0] == 1);
  CHECK(padded_ok.image.samples[1] == 2);
  CHECK(padded_ok.image.stride == static_cast<int>(even_only.size()) + 1);
  const auto two_rows = Encode(kMediaMono12Packed, RawByteOrder::LittleEndian, 2, 2, {pair, pair + 4}, 0);
  auto dangling = two_rows;
  dangling.push_back(0x5A);
  ExpectStatus(Decode(kMediaMono12Packed, RawByteOrder::LittleEndian, 2, 2, dangling, 0), RawDecodeStatus::WrongLength);
  ExpectStatus(Decode(kMediaMono16, RawByteOrder::LittleEndian, 2, 2, full16, 4, 0), RawDecodeStatus::WrongLength);
  ExpectStatus(Decode(kMediaMono16, RawByteOrder::LittleEndian, 1, 1, full16, 4, 2), RawDecodeStatus::WrongLength);
  ExpectStatus(Decode(kMediaMono8, RawByteOrder::LittleEndian, 4, 1, mono8, 2, 1), RawDecodeStatus::WrongLength);

  RawDecodeRequest null_request;
  null_request.media_type = kMediaMono16;
  null_request.width = 1;
  null_request.height = 1;
  null_request.data = nullptr;
  null_request.size = 2;
  ExpectStatus(DecodeRawFrame(null_request), RawDecodeStatus::NullBuffer);
  null_request.size = 0;
  ExpectStatus(DecodeRawFrame(null_request), RawDecodeStatus::NullBuffer);

  RawDecodeRequest bad_geom = null_request;
  bad_geom.data = full16;
  bad_geom.size = 4;
  bad_geom.width = 0;
  ExpectStatus(DecodeRawFrame(bad_geom), RawDecodeStatus::InvalidGeometry);
  bad_geom.width = -3;
  bad_geom.height = 2;
  ExpectStatus(DecodeRawFrame(bad_geom), RawDecodeStatus::InvalidGeometry);
  bad_geom.width = 2;
  bad_geom.height = -1;
  ExpectStatus(DecodeRawFrame(bad_geom), RawDecodeStatus::InvalidGeometry);
  bad_geom.height = 1;
  bad_geom.stride = -4;
  ExpectStatus(DecodeRawFrame(bad_geom), RawDecodeStatus::InvalidGeometry);
  bad_geom.stride = 0;
  bad_geom.width = tape_scanner::kMaxRawDimension;
  bad_geom.height = 2;
  ExpectStatus(DecodeRawFrame(bad_geom), RawDecodeStatus::InvalidGeometry);

  const uint8_t one[] = {0x10};
  ExpectStatus(Decode(0, RawByteOrder::LittleEndian, 1, 1, one, 1, 0), RawDecodeStatus::UnsupportedFormat);
  ExpectStatus(Decode(0x01100003u, RawByteOrder::LittleEndian, 1, 1, low12, 2, 0), RawDecodeStatus::UnsupportedFormat);
  ExpectStatus(Decode(0x01080008u, RawByteOrder::LittleEndian, 1, 1, one, 1, 0), RawDecodeStatus::UnsupportedFormat);
  ExpectStatus(Decode(0x02180014u, RawByteOrder::LittleEndian, 1, 1, one, 1, 0), RawDecodeStatus::UnsupportedFormat);
  ExpectStatus(Decode(0x02080069u, RawByteOrder::LittleEndian, 1, 1, one, 1, 0), RawDecodeStatus::UnsupportedFormat);
  ExpectStatus(Decode(0x010C0060u, RawByteOrder::LittleEndian, 1, 1, dirty_tail, 2, 0), RawDecodeStatus::UnsupportedFormat);
  ExpectStatus(Decode(kMediaMono12p + 1, RawByteOrder::LittleEndian, 1, 1, low12, 2, 0), RawDecodeStatus::UnsupportedFormat);

  // Padding 0xA5 не становится пикселем. Две строки с разным stride дают один канонический ряд.
  const uint16_t rows[] = {0, 1, 17, 18, 256, 4095, 3, 4, 5, 6, 7, 8, 9, 10};
  const auto padded = Encode(kMediaMono12p, RawByteOrder::LittleEndian, 7, 2, {rows, rows + 14}, 4);
  const auto tight = Encode(kMediaMono12p, RawByteOrder::LittleEndian, 7, 2, {rows, rows + 14}, 0);
  const auto a = Decode(kMediaMono12p, RawByteOrder::LittleEndian, 7, 2, padded, 0);
  const auto b = Decode(kMediaMono12p, RawByteOrder::LittleEndian, 7, 2, tight, 0);
  CHECK(a.status == RawDecodeStatus::Ok);
  CHECK(b.status == RawDecodeStatus::Ok);
  CHECK(a.image.samples == b.image.samples);
  CHECK(a.image.stride != b.image.stride);
  CHECK(a.image.samples[0] == 0);
  CHECK(a.image.samples[5] == 4095);
  CHECK(a.image.samples[6] == 3);
  CHECK(a.image.samples[7] == 4);

  CHECK(std::string(tape_scanner::RawDecodeStatusText(RawDecodeStatus::WrongLength)) == "wrong length");
  CHECK(std::string(tape_scanner::RawDecodeStatusText(RawDecodeStatus::UnsupportedFormat)) == "unsupported format");

  if (g_failures != 0) {
    std::cerr << g_failures << " failed, " << g_checks << " checks\n";
    return 1;
  }
  std::cout << g_checks << " checks passed\n";
  return 0;
}

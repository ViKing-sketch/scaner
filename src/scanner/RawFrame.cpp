#include <tape_scanner/scanner/RawFrame.hpp>

#include <limits>

namespace tape_scanner {
namespace {

enum class Layout { Mono8, Mono12, Mono16, PackedGigE, PackedMv, PackedP };

struct FormatInfo {
  Layout layout;
  int significant_bits;
};

bool Lookup(uint32_t media_type, FormatInfo &info) {
  if (media_type == kMediaMono8) {
    info = {Layout::Mono8, 8};
    return true;
  }
  if (media_type == kMediaMono12) {
    info = {Layout::Mono12, 12};
    return true;
  }
  if (media_type == kMediaMono16) {
    info = {Layout::Mono16, 16};
    return true;
  }
  if (media_type == kMediaMono12Packed) {
    info = {Layout::PackedGigE, 12};
    return true;
  }
  if (media_type == kMediaMono12PackedMv) {
    info = {Layout::PackedMv, 12};
    return true;
  }
  if (media_type == kMediaMono12p) {
    info = {Layout::PackedP, 12};
    return true;
  }
  return false;
}

int MinRowBytes(Layout layout, int width) {
  switch (layout) {
  case Layout::Mono8:
    return width;
  case Layout::Mono12:
  case Layout::Mono16:
    return width * 2;
  case Layout::PackedGigE:
  case Layout::PackedMv:
  case Layout::PackedP:
    return (width / 2) * 3 + (width % 2) * 2;
  }
  return 0;
}

uint16_t ReadU16(const uint8_t *p, RawByteOrder order) {
  if (order == RawByteOrder::LittleEndian) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
  }
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

RawDecodeStatus DecodeRow(Layout layout, RawByteOrder order, const uint8_t *row, int width, uint16_t *dst) {
  if (layout == Layout::Mono8) {
    for (int x = 0; x < width; ++x) {
      dst[x] = row[x];
    }
    return RawDecodeStatus::Ok;
  }
  if (layout == Layout::Mono12 || layout == Layout::Mono16) {
    for (int x = 0; x < width; ++x) {
      const uint16_t value = ReadU16(row + static_cast<std::size_t>(x) * 2, order);
      if (layout == Layout::Mono12 && (value & 0xF000u) != 0) {
        return RawDecodeStatus::InvalidPacking;
      }
      dst[x] = value;
    }
    return RawDecodeStatus::Ok;
  }

  (void)order;
  int x = 0;
  const uint8_t *p = row;
  while (x + 1 < width) {
    const uint8_t b0 = p[0];
    const uint8_t b1 = p[1];
    const uint8_t b2 = p[2];
    uint16_t p0 = 0;
    uint16_t p1 = 0;
    if (layout == Layout::PackedGigE) {
      p0 = static_cast<uint16_t>((static_cast<uint16_t>(b0) << 4) | (b1 & 0x0Fu));
      p1 = static_cast<uint16_t>((static_cast<uint16_t>(b2) << 4) | (b1 >> 4));
    } else if (layout == Layout::PackedMv) {
      p0 = static_cast<uint16_t>((static_cast<uint16_t>(b0) << 4) | (b1 >> 4));
      p1 = static_cast<uint16_t>((static_cast<uint16_t>(b2) << 4) | (b1 & 0x0Fu));
    } else {
      p0 = static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1 & 0x0Fu) << 8));
      p1 = static_cast<uint16_t>((b1 >> 4) | (static_cast<uint16_t>(b2) << 4));
    }
    dst[x] = p0;
    dst[x + 1] = p1;
    p += 3;
    x += 2;
  }
  if (x < width) {
    const uint8_t b0 = p[0];
    const uint8_t b1 = p[1];
    if (layout == Layout::PackedGigE) {
      if ((b1 & 0xF0u) != 0) {
        return RawDecodeStatus::InvalidPacking;
      }
      dst[x] = static_cast<uint16_t>((static_cast<uint16_t>(b0) << 4) | (b1 & 0x0Fu));
    } else if (layout == Layout::PackedMv) {
      if ((b1 & 0x0Fu) != 0) {
        return RawDecodeStatus::InvalidPacking;
      }
      dst[x] = static_cast<uint16_t>((static_cast<uint16_t>(b0) << 4) | (b1 >> 4));
    } else {
      if ((b1 & 0xF0u) != 0) {
        return RawDecodeStatus::InvalidPacking;
      }
      dst[x] = static_cast<uint16_t>(b0 | (static_cast<uint16_t>(b1 & 0x0Fu) << 8));
    }
  }
  return RawDecodeStatus::Ok;
}

} // namespace

bool RawFormatSupported(uint32_t media_type) {
  FormatInfo info;
  return Lookup(media_type, info);
}

int RawSignificantBits(uint32_t media_type) {
  FormatInfo info;
  if (!Lookup(media_type, info)) {
    return 0;
  }
  return info.significant_bits;
}

const char *RawDecodeStatusText(RawDecodeStatus status) {
  switch (status) {
  case RawDecodeStatus::Ok:
    return "ok";
  case RawDecodeStatus::NullBuffer:
    return "null buffer";
  case RawDecodeStatus::UnsupportedFormat:
    return "unsupported format";
  case RawDecodeStatus::InvalidGeometry:
    return "invalid geometry";
  case RawDecodeStatus::WrongLength:
    return "wrong length";
  case RawDecodeStatus::InvalidPacking:
    return "invalid packing";
  case RawDecodeStatus::UnsupportedByteOrder:
    return "unsupported byte order";
  }
  return "unknown";
}

RawDecodeResult DecodeRawFrame(const RawDecodeRequest &request) {
  RawDecodeResult result;
  if (request.data == nullptr) {
    result.status = RawDecodeStatus::NullBuffer;
    return result;
  }
  if (request.width <= 0 || request.height <= 0 || request.width > kMaxRawDimension ||
      request.height > kMaxRawDimension || request.stride < 0) {
    result.status = RawDecodeStatus::InvalidGeometry;
    return result;
  }
  const auto sample_count = static_cast<std::uint64_t>(request.width) * static_cast<std::uint64_t>(request.height);
  if (sample_count > kMaxRawSamples) {
    result.status = RawDecodeStatus::InvalidGeometry;
    return result;
  }

  FormatInfo info;
  if (!Lookup(request.media_type, info)) {
    result.status = RawDecodeStatus::UnsupportedFormat;
    return result;
  }
  if (request.byte_order == RawByteOrder::BigEndian && info.layout != Layout::Mono12 && info.layout != Layout::Mono16) {
    result.status = RawDecodeStatus::UnsupportedByteOrder;
    return result;
  }

  const int min_row = MinRowBytes(info.layout, request.width);
  if (min_row <= 0) {
    result.status = RawDecodeStatus::InvalidGeometry;
    return result;
  }

  int stride = request.stride;
  if (stride == 0) {
    if (request.size % static_cast<std::size_t>(request.height) != 0) {
      result.status = RawDecodeStatus::WrongLength;
      return result;
    }
    const std::size_t inferred = request.size / static_cast<std::size_t>(request.height);
    if (inferred < static_cast<std::size_t>(min_row) || inferred > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      result.status = RawDecodeStatus::WrongLength;
      return result;
    }
    stride = static_cast<int>(inferred);
  }

  const auto need = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(request.height);
  if (stride < min_row || need != request.size) {
    result.status = RawDecodeStatus::WrongLength;
    return result;
  }

  std::vector<uint16_t> samples(static_cast<std::size_t>(sample_count));
  for (int y = 0; y < request.height; ++y) {
    const uint8_t *row = request.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
    const RawDecodeStatus row_status =
        DecodeRow(info.layout, request.byte_order, row, request.width, samples.data() + static_cast<std::size_t>(y) * request.width);
    if (row_status != RawDecodeStatus::Ok) {
      result.status = row_status;
      return result;
    }
  }

  result.status = RawDecodeStatus::Ok;
  result.image.media_type = request.media_type;
  result.image.width = request.width;
  result.image.height = request.height;
  result.image.stride = stride;
  result.image.significant_bits = info.significant_bits;
  result.image.stored_bits = 16;
  result.image.byte_order = request.byte_order;
  result.image.samples = std::move(samples);
  return result;
}

} // namespace tape_scanner

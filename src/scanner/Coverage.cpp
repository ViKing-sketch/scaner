#include <tape_scanner/scanner/Coverage.hpp>

#include <cstdint>
#include <fstream>
#include <limits>

namespace {

void WriteU32(std::ostream &out, std::uint32_t value) {
  const unsigned char bytes[4] = {
      static_cast<unsigned char>(value & 0xffu),
      static_cast<unsigned char>((value >> 8) & 0xffu),
      static_cast<unsigned char>((value >> 16) & 0xffu),
      static_cast<unsigned char>((value >> 24) & 0xffu),
  };
  out.write(reinterpret_cast<const char *>(bytes), 4);
}

bool ReadU32(std::istream &in, std::uint32_t &value) {
  unsigned char bytes[4];
  if (!in.read(reinterpret_cast<char *>(bytes), 4)) {
    return false;
  }
  value = static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) |
          (static_cast<std::uint32_t>(bytes[2]) << 16) | (static_cast<std::uint32_t>(bytes[3]) << 24);
  return true;
}

} // namespace

IntensityStats IntensityOnCoverage(const cv::Mat &image, const cv::Mat &coverage) {
  IntensityStats stats;
  stats.min_value = std::numeric_limits<double>::quiet_NaN();
  stats.max_value = std::numeric_limits<double>::quiet_NaN();
  stats.mean = std::numeric_limits<double>::quiet_NaN();
  if (image.empty() || image.type() != CV_16UC1 || coverage.type() != CV_8UC1 || coverage.size() != image.size()) {
    return stats;
  }

  double sum = 0;
  bool any = false;
  for (int row = 0; row < image.rows; ++row) {
    const auto *pixels = image.ptr<std::uint16_t>(row);
    const auto *mask = coverage.ptr<std::uint8_t>(row);
    for (int col = 0; col < image.cols; ++col) {
      if (mask[col] == kCoverageUnfilled) {
        ++stats.invalid_count;
        continue;
      }
      const double sample = pixels[col];
      ++stats.valid_count;
      sum += sample;
      if (!any) {
        stats.min_value = sample;
        stats.max_value = sample;
        any = true;
      } else {
        if (sample < stats.min_value) {
          stats.min_value = sample;
        }
        if (sample > stats.max_value) {
          stats.max_value = sample;
        }
      }
    }
  }

  if (!any) {
    return stats;
  }
  stats.available = true;
  stats.mean = sum / static_cast<double>(stats.valid_count);
  return stats;
}

std::string CoverageMaskPath(const std::string &dicom_path) { return dicom_path + ".coverage"; }

bool WriteCoverageMask(const std::string &path, const cv::Mat &coverage, const std::string &gap_reason) {
  if (coverage.empty() || coverage.type() != CV_8UC1 || coverage.rows <= 0 || coverage.cols <= 0) {
    return false;
  }
  if (gap_reason.size() > 255) {
    return false;
  }

  const cv::Mat continuous = coverage.isContinuous() ? coverage : coverage.clone();
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(kCoverageMagic, 4);
  const unsigned char version = kCoverageFileVersion;
  const unsigned char reason_len = static_cast<unsigned char>(gap_reason.size());
  out.write(reinterpret_cast<const char *>(&version), 1);
  out.write(reinterpret_cast<const char *>(&reason_len), 1);
  WriteU32(out, static_cast<std::uint32_t>(continuous.rows));
  WriteU32(out, static_cast<std::uint32_t>(continuous.cols));
  if (reason_len != 0) {
    out.write(gap_reason.data(), static_cast<std::streamsize>(reason_len));
  }
  out.write(reinterpret_cast<const char *>(continuous.data),
            static_cast<std::streamsize>(continuous.rows) * continuous.cols);
  out.flush();
  return static_cast<bool>(out);
}

bool ReadCoverageMask(const std::string &path, cv::Mat &coverage, std::string &gap_reason) {
  coverage.release();
  gap_reason.clear();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  char magic[4];
  if (!in.read(magic, 4) || std::string(magic, 4) != kCoverageMagic) {
    return false;
  }
  unsigned char version = 0;
  unsigned char reason_len = 0;
  if (!in.read(reinterpret_cast<char *>(&version), 1) || !in.read(reinterpret_cast<char *>(&reason_len), 1) ||
      version != kCoverageFileVersion) {
    return false;
  }
  std::uint32_t rows = 0;
  std::uint32_t cols = 0;
  if (!ReadU32(in, rows) || !ReadU32(in, cols) || rows == 0 || cols == 0) {
    return false;
  }
  if (rows > 65535u || cols > 65535u) {
    return false;
  }
  std::string reason(reason_len, '\0');
  if (reason_len != 0 && !in.read(reason.data(), reason_len)) {
    return false;
  }
  cv::Mat mask(static_cast<int>(rows), static_cast<int>(cols), CV_8UC1);
  if (!in.read(reinterpret_cast<char *>(mask.data), static_cast<std::streamsize>(rows) * cols)) {
    return false;
  }
  char extra = 0;
  if (in.read(&extra, 1)) {
    return false;
  }
  coverage = std::move(mask);
  gap_reason = std::move(reason);
  return true;
}

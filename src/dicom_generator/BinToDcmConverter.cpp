#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <dcmtk/dcmdata/dcuid.h>
#include <opencv2/core.hpp>

#include <tape_scanner/dicom_generator/BinToDcmConverter.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/Utils.hpp>

namespace tape_scanner::dicom_generator {

using std::runtime_error;

namespace {

bool MultiplySizes(size_t a, size_t b, size_t &product) {
  if (a != 0 && b > std::numeric_limits<size_t>::max() / a) {
    return false;
  }
  product = a * b;
  return true;
}

std::string FramePath(const std::string &path, size_t instance) {
  if (instance <= 1) {
    return path;
  }
  const auto slash = path.find_last_of("/\\");
  const auto dot = path.find_last_of('.');
  const bool has_ext = dot != std::string::npos && (slash == std::string::npos || dot > slash);
  const std::string stem = has_ext ? path.substr(0, dot) : path;
  const std::string ext = has_ext ? path.substr(dot) : std::string(".dcm");
  std::string index = std::to_string(instance);
  if (index.size() < 3) {
    index.insert(0, 3 - index.size(), '0');
  }
  return stem + "_" + index + ext;
}

cv::Mat PixelsToMat(const uint8_t *bytes, size_t width, size_t height, size_t bits) {
  cv::Mat image(static_cast<int>(height), static_cast<int>(width), CV_16UC1);
  if (bits > 8) {
    std::memcpy(image.data, bytes, width * height * sizeof(uint16_t));
  } else {
    uint16_t *dst = image.ptr<uint16_t>();
    const size_t count = width * height;
    for (size_t i = 0; i < count; ++i) {
      dst[i] = bytes[i];
    }
  }
  return image;
}

bool ReadExact(std::ifstream &in, char *dst, size_t count) {
  in.read(dst, static_cast<std::streamsize>(count));
  return static_cast<size_t>(in.gcount()) == count;
}

} // namespace

std::vector<std::string> ConvertBinToDcm(string in_bin_file_path, size_t width, size_t height, size_t bits,
                                         size_t frames, bool all_frames_as_1st,
                                         std::function<void(const string &msg)> log_callback,
                                         double pixel_spacing_row_mm, double pixel_spacing_col_mm,
                                         string_opt out_dcm_file_path) {
  if (!std::isfinite(pixel_spacing_row_mm) || !(pixel_spacing_row_mm > 0.0) ||
      !std::isfinite(pixel_spacing_col_mm) || !(pixel_spacing_col_mm > 0.0)) {
    throw runtime_error("ERROR: pixel spacing must be a positive finite parameter, in millimetres.\n");
  }
  if (bits != 8 && bits != 16) {
    throw runtime_error("ERROR: only 8 or 16 bits per pixel.\n");
  }
  if (width == 0 || height == 0 || frames == 0) {
    throw runtime_error("ERROR: width, height and frames must be positive.\n");
  }
  if (width > static_cast<size_t>(kMaxDicomDimension) || height > static_cast<size_t>(kMaxDicomDimension) ||
      frames > static_cast<size_t>(UINT16_MAX)) {
    throw runtime_error("ERROR: width, height or frame count is outside the DICOM frame limit.\n");
  }

  const size_t bytes_per_pixel = bits / 8;
  size_t pixels = 0;
  size_t frame_bin_size = 0;
  size_t stored_bytes = 0;
  if (!MultiplySizes(width, height, pixels) || !MultiplySizes(pixels, bytes_per_pixel, frame_bin_size) ||
      !MultiplySizes(frame_bin_size, frames, stored_bytes)) {
    throw runtime_error("ERROR: width, height and frame count overflow.\n");
  }
  // «Все кадры как первый» копирует один кадр. На диске должна лежать ровно одна рамка.
  const size_t expected_file_bytes = all_frames_as_1st ? frame_bin_size : stored_bytes;

  log_callback(std::format("Pixel spacing parameter: {}\\{} mm\n", pixel_spacing_row_mm, pixel_spacing_col_mm));
  log_callback(std::format("Expected input size: {} bytes\n", expected_file_bytes));

  const string dest_file_path = out_dcm_file_path.value_or(in_bin_file_path + ".dcm");

  std::ifstream in(in_bin_file_path, std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    throw runtime_error("ERROR: Can't open input file for reading.\n");
  }

  in.seekg(0, std::ios::end);
  const std::streamoff end = in.tellg();
  if (end < 0) {
    throw runtime_error("ERROR: short read, input size is unknown.\n");
  }
  const auto src_file_size = static_cast<size_t>(end);
  in.seekg(0);
  log_callback(std::format("Input file size: {} bytes\n", src_file_size));
  if (src_file_size < expected_file_bytes) {
    throw runtime_error("ERROR: short read, input file is smaller than the declared frame.\n");
  }
  if (src_file_size != expected_file_bytes) {
    throw runtime_error("ERROR: input file byte count does not match width, height, bit depth and frames.\n");
  }

  std::vector<uint8_t> buf(stored_bytes);
  if (all_frames_as_1st) {
    for (size_t frame = 0; frame < frames; ++frame) {
      in.seekg(0);
      if (!ReadExact(in, reinterpret_cast<char *>(buf.data() + frame_bin_size * frame), frame_bin_size)) {
        throw runtime_error("ERROR: short read while copying the first frame.\n");
      }
    }
  } else if (!ReadExact(in, reinterpret_cast<char *>(buf.data()), stored_bytes)) {
    throw runtime_error("ERROR: short read while loading the input file.\n");
  }

  DicomWriteParams params;
  char study_uid[100];
  char series_uid[100];
  params.study_instance_uid = dcmGenerateUniqueIdentifier(study_uid, SITE_STUDY_UID_ROOT);
  params.series_instance_uid = dcmGenerateUniqueIdentifier(series_uid, SITE_SERIES_UID_ROOT);
  params.pixel_spacing_row_mm = pixel_spacing_row_mm;
  params.pixel_spacing_col_mm = pixel_spacing_col_mm;

  std::vector<std::string> written;
  written.reserve(frames);
  for (size_t frame = 0; frame < frames; ++frame) {
    cv::Mat image = PixelsToMat(buf.data() + frame_bin_size * frame, width, height, bits);
    params.instance_number = static_cast<int>(frame + 1);
    params.sop_instance_uid.clear();
    const string path = FramePath(dest_file_path, frame + 1);
    if (!saveToDcm(image, path, params)) {
      throw runtime_error("ERROR: Failed to save resulting DICOM file.\n");
    }
    written.push_back(path);
    log_callback(std::format("Saved {}\n", path));
  }
  return written;
}

} // namespace tape_scanner::dicom_generator

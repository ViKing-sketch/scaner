#include <cstring>
#include <format>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <dcmtk/dcmdata/dcuid.h>
#include <opencv2/core.hpp>

#include <tape_scanner/dicom_generator/BinToDcmConverter.hpp>
#include <tape_scanner/scanner/Utils.hpp>

namespace tape_scanner::dicom_generator {

using std::runtime_error;

namespace {

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

} // namespace

std::vector<std::string> ConvertBinToDcm(string in_bin_file_path, size_t width, size_t height, size_t bits,
                                             size_t frames, bool all_frames_as_1st,
                                             std::function<void(const string &msg)> log_callback,
                                             string_opt out_dcm_file_path, double pixel_spacing_row_mm,
                                             double pixel_spacing_col_mm) {
  if (bits != 8 && bits != 16) {
    throw runtime_error("ERROR: only 8 or 16 bits per pixel.\n");
  }
  if (!width || !height || !frames) {
    throw runtime_error("ERROR: width, height and frames must be positive.\n");
  }

  const size_t frame_bin_size = width * height * (bits / 8);
  const size_t src_file_size_min = frame_bin_size * frames;
  log_callback(std::format("Input file minimum size: {} bytes\n", src_file_size_min));

  const string dest_file_path = out_dcm_file_path.value_or(in_bin_file_path + ".dcm");

  std::ifstream in(in_bin_file_path, std::ios::in | std::ios::binary);
  if (!in.is_open()) {
    throw runtime_error("ERROR: Can't open input file for reading.\n");
  }

  in.seekg(0, std::ios_base::end);
  const auto src_file_size = static_cast<size_t>(in.tellg());
  in.seekg(0);
  log_callback(std::format("Input file size: {} bytes\n", src_file_size));
  if (src_file_size < src_file_size_min) {
    throw runtime_error("ERROR: Input file size is too small.\n");
  }

  std::vector<uint8_t> buf(src_file_size_min);
  if (all_frames_as_1st) {
    for (size_t frame = 0; frame < frames; ++frame) {
      in.seekg(0);
      if (in.read(reinterpret_cast<char *>(buf.data() + frame_bin_size * frame),
                  static_cast<std::streamsize>(frame_bin_size))
              .bad()) {
        throw runtime_error("ERROR: Failed to read input file into RAM buffer.\n");
      }
    }
  } else if (in.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(src_file_size_min)).bad()) {
    throw runtime_error("ERROR: Failed to read whole input file into RAM buffer.\n");
  }

  // Единственная запись DICOM — saveToDcm. Шаг приходит от вызывающего кода.
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

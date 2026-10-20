#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <dcmtk/dcmdata/dcuid.h>
#include <opencv2/core.hpp>

#include <tape_scanner/scanner/Utils.hpp>

#include "DicomGeneratorUtils.hpp"

namespace tape_scanner::dicom_generator {

using std::runtime_error;
using std::string;
using std::vector;

/**
 * @brief CreateDcm - создать тестовый DCM файл по заданным параметрам.
 * Пиксели пишутся только через saveToDcm (Utils.cpp). Pixel Spacing берётся из DicomWriteParams.
 * @param dest_file_path - путь до результирующего файла. При frames > 1 первый кадр
 *        пишется сюда, следующие — с суффиксом _002, _003, …
 * @param width - ширина фрейма.
 * @param height - высота фрейма.
 * @param bits - количесво бит для хранения одного пикселя (8 или 16).
 * @param frames - количество фреймов.
 * @param fill_value - каким значением заполнить каждый пиксель.
 * @param fill_value_max - максимальное значение пикселя. Используется при инкрементировании.
 * @param fill_value_increment - на какое значение инкрементировать значение следующего пикселя.
 * @param increment_for_new_frames_only - инкрементировать все значения пикселей только для каждого нового фрейма.
 */
inline void CreateDcm(string dest_file_path, size_t width, size_t height, size_t bits, size_t frames,
                      size_t fill_value, size_t fill_value_max, size_t fill_value_increment,
                      bool increment_for_new_frames_only) {
  if (bits != 8 && bits != 16) {
    throw runtime_error("ERROR: only 8 or 16 bits per pixel.\n");
  }

  if (!width) {
    width = 1;
  } else if (width > FRAME_MAX_WIDTH) {
    width = FRAME_MAX_WIDTH;
  }

  if (!height) {
    height = 1;
  } else if (height > FRAME_MAX_HEIGHT) {
    height = FRAME_MAX_HEIGHT;
  }

  if (!frames) {
    frames = 1;
  } else if (frames > FRAME_MAX_COUNT) {
    frames = FRAME_MAX_COUNT;
  }

  const size_t frame_bin_size = width * height * (bits / 8);
  const size_t frames_bin_size = frame_bin_size * frames;
  vector<uint8_t> buf(frames_bin_size);

  if (fill_value == UINT32_MAX) {
    std::generate(buf.begin(), buf.end(), []() { return static_cast<uint8_t>(rand() % 256); });
  } else {
    size_t value = fill_value;
    for (size_t frame = 0; frame < frames; ++frame) {
      const size_t bytes_per_value = bits / 8;
      for (size_t byte = 0; byte < frame_bin_size; byte += bytes_per_value) {
        if (bytes_per_value > 1) {
          *reinterpret_cast<uint16_t *>(&buf[frame_bin_size * frame + byte]) = static_cast<uint16_t>(value);
        } else {
          buf[frame_bin_size * frame + byte] = static_cast<uint8_t>(value);
        }
        if (fill_value_increment && !increment_for_new_frames_only) {
          value += fill_value_increment;
          if (value > fill_value_max) {
            value = 0;
          }
        }
      }
      if (fill_value_increment && increment_for_new_frames_only) {
        value += fill_value_increment;
        if (value > fill_value_max) {
          value = 0;
        }
      }
    }
  }

  DicomWriteParams params;
  char study_uid[100];
  char series_uid[100];
  params.study_instance_uid = dcmGenerateUniqueIdentifier(study_uid, SITE_STUDY_UID_ROOT);
  params.series_instance_uid = dcmGenerateUniqueIdentifier(series_uid, SITE_SERIES_UID_ROOT);

  auto frame_path = [&](size_t instance) -> string {
    if (instance <= 1) {
      return dest_file_path;
    }
    const auto slash = dest_file_path.find_last_of("/\\");
    const auto dot = dest_file_path.find_last_of('.');
    const bool has_ext = dot != string::npos && (slash == string::npos || dot > slash);
    const string stem = has_ext ? dest_file_path.substr(0, dot) : dest_file_path;
    const string ext = has_ext ? dest_file_path.substr(dot) : string(".dcm");
    string index = std::to_string(instance);
    if (index.size() < 3) {
      index.insert(0, 3 - index.size(), '0');
    }
    return stem + "_" + index + ext;
  };

  for (size_t frame = 0; frame < frames; ++frame) {
    cv::Mat image(static_cast<int>(height), static_cast<int>(width), CV_16UC1);
    const uint8_t *src = buf.data() + frame_bin_size * frame;
    if (bits > 8) {
      std::memcpy(image.data, src, frame_bin_size);
    } else {
      uint16_t *dst = image.ptr<uint16_t>();
      const size_t count = width * height;
      for (size_t i = 0; i < count; ++i) {
        dst[i] = src[i];
      }
    }
    params.instance_number = static_cast<int>(frame + 1);
    params.sop_instance_uid.clear();
    if (!saveToDcm(image, frame_path(frame + 1), params)) {
      throw runtime_error("ERROR: Failed to save resulting DICOM file.\n");
    }
  }
}

} // namespace tape_scanner::dicom_generator

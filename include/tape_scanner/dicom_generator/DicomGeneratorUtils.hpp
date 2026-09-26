#pragma once

#include <cstddef>
#include <cstdint>

namespace tape_scanner::dicom_generator {

constexpr size_t FRAME_MAX_WIDTH = UINT16_MAX;
constexpr size_t FRAME_MAX_HEIGHT = UINT16_MAX;
constexpr size_t FRAME_MAX_COUNT = UINT16_MAX;


size_t CalcFramesSize(size_t width, size_t height, size_t bits, size_t frames) {
  size_t frame_bin_size = width * height * (bits / 8);
  return frame_bin_size * frames;
}

} // namespace tape_scanner::dicom_generator

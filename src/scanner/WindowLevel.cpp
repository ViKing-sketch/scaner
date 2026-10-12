#include <tape_scanner/scanner/WindowLevel.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#include <cstring>

cv::Mat ReadStoredMono16(const std::string &path, int frame_index) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str(), EXS_Unknown, EGL_noChange, 0x7fffffff).bad()) {
    return {};
  }
  DcmDataset *dataset = file.getDataset();
  Uint16 samples = 1;
  Uint16 bits_allocated = 0;
  Uint16 rows = 0;
  Uint16 cols = 0;
  dataset->findAndGetUint16(DCM_SamplesPerPixel, samples);
  dataset->findAndGetUint16(DCM_BitsAllocated, bits_allocated);
  if (samples != 1 || bits_allocated != 16) {
    return {};
  }
  if (dataset->findAndGetUint16(DCM_Rows, rows).bad() || dataset->findAndGetUint16(DCM_Columns, cols).bad()) {
    return {};
  }
  const Uint16 *data = nullptr;
  unsigned long count = 0;
  if (dataset->findAndGetUint16Array(DCM_PixelData, data, &count).bad() || data == nullptr || rows == 0 || cols == 0) {
    return {};
  }
  const unsigned long frame_pixels = static_cast<unsigned long>(rows) * cols;
  if (frame_index < 0 || count < frame_pixels * static_cast<unsigned long>(frame_index + 1)) {
    return {};
  }
  cv::Mat image(rows, cols, CV_16UC1);
  std::memcpy(image.data, data + frame_pixels * static_cast<unsigned long>(frame_index), frame_pixels * sizeof(Uint16));
  return image;
}

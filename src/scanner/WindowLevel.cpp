#include <tape_scanner/scanner/WindowLevel.hpp>
#include <tape_scanner/scanner/DicomScan.hpp>

cv::Mat ReadStoredMono16(const std::string &path, int frame_index) {
  DicomScan scan;
  std::string error;
  if (!LoadDicomScan(path, scan, error)) {
    return {};
  }
  if (frame_index < 0 || frame_index >= static_cast<int>(scan.frames.size())) {
    return {};
  }
  const cv::Mat &frame = scan.frames[static_cast<size_t>(frame_index)];
  if (frame.type() != CV_16UC1) {
    return {};
  }
  return frame;
}

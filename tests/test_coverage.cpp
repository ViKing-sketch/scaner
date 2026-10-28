#include <tape_scanner/scanner/Coverage.hpp>
#include <tape_scanner/scanner/Film.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <opencv2/core.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool ok, const char *expr, int line) {
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

bool Near(double a, double b) { return std::abs(a - b) < 1e-6; }

ScanGeometry Geometry() {
  ScanGeometry geometry;
  geometry.bin = 1;
  geometry.pixel_um_x = kNominalPixelUm;
  geometry.pixel_um_y = kNominalPixelUm;
  return geometry;
}

bool LoadPixels(const std::string &path, cv::Mat &out) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str()).bad()) {
    return false;
  }
  DcmDataset *dataset = file.getDataset();
  Uint16 rows = 0;
  Uint16 cols = 0;
  if (dataset->findAndGetUint16(DCM_Rows, rows).bad() || dataset->findAndGetUint16(DCM_Columns, cols).bad()) {
    return false;
  }
  const Uint16 *data = nullptr;
  unsigned long count = 0;
  if (dataset->findAndGetUint16Array(DCM_PixelData, data, &count).bad() || data == nullptr) {
    return false;
  }
  if (count != static_cast<unsigned long>(rows) * cols) {
    return false;
  }
  out.create(rows, cols, CV_16UC1);
  std::memcpy(out.data, data, count * sizeof(std::uint16_t));
  return true;
}

/// Дырка между тайлами остаётся нулём в DICOM и нулём в sidecar. Статистика её не видит.
void TestUnfilledSidecarSkippedByStats() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_coverage_hole";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  const std::string chosen = (dir / "hole.dcm").string();
  ScanSession session(chosen, Geometry());
  cv::Mat left(3, 2, CV_16UC1, cv::Scalar(1000));
  cv::Mat right(3, 2, CV_16UC1, cv::Scalar(3000));
  CHECK(session.PushRegion(left, 0, 0));
  CHECK(session.PushRegion(right, 4, 0));
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 1);
  CHECK(session.Coverage().size() == 1);
  CHECK(!session.Coverage()[0].complete);
  CHECK(session.Coverage()[0].gap_reason == kCoverageGapUnfilled);

  cv::Mat pixels;
  cv::Mat mask;
  std::string reason;
  CHECK(LoadPixels(session.SavedPaths()[0], pixels));
  CHECK(ReadCoverageMask(session.Coverage()[0].mask_path, mask, reason));
  CHECK(session.Coverage()[0].mask_path == CoverageMaskPath(session.SavedPaths()[0]));
  CHECK(reason == kCoverageGapUnfilled);
  CHECK(pixels.rows == 3);
  CHECK(pixels.cols == 6);
  CHECK(pixels.at<std::uint16_t>(1, 2) == 0);
  CHECK(mask.at<std::uint8_t>(1, 2) == kCoverageUnfilled);
  CHECK(pixels.at<std::uint16_t>(1, 0) == 1000);
  CHECK(mask.at<std::uint8_t>(1, 0) == kCoverageValid);
  CHECK(pixels.at<std::uint16_t>(1, 4) == 3000);
  CHECK(mask.at<std::uint8_t>(1, 4) == kCoverageValid);

  double raw_min = 0;
  double raw_max = 0;
  cv::minMaxLoc(pixels, &raw_min, &raw_max);
  CHECK(raw_min == 0);
  const IntensityStats stats = IntensityOnCoverage(pixels, mask);
  CHECK(stats.available);
  CHECK(stats.min_value == 1000);
  CHECK(stats.max_value == 3000);
  CHECK(Near(stats.mean, 2000));
  CHECK(stats.invalid_count == 6);
  CHECK(stats.valid_count == 12);
  CHECK(session.Coverage()[0].stats.min_value == 1000);
  CHECK(Near(session.Coverage()[0].stats.mean, 2000));

  std::filesystem::remove_all(dir);
}

/// Ноль внутри тайла — измеренный отсчёт: маска 255, минимум равен нулю.
void TestMeasuredZeroCounts() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_coverage_zero";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  Film black_film;
  cv::Mat black(4, 4, CV_16UC1, cv::Scalar(0));
  black_film.PushRegion(black, 0, 0);
  const CoveredFrame zeros = black_film.RenderCovered();
  CHECK(zeros.complete);
  CHECK(zeros.gap_reason.empty());
  CHECK(zeros.stats.available);
  CHECK(zeros.stats.min_value == 0);
  CHECK(zeros.stats.max_value == 0);
  CHECK(zeros.stats.mean == 0);
  CHECK(zeros.stats.invalid_count == 0);
  CHECK(zeros.stats.valid_count == 16);
  CHECK(zeros.coverage.at<std::uint8_t>(2, 2) == kCoverageValid);

  const std::string chosen = (dir / "zero.dcm").string();
  ScanSession session(chosen, Geometry());
  cv::Mat tile(2, 2, CV_16UC1, cv::Scalar(8));
  tile.at<std::uint16_t>(0, 0) = 0;
  CHECK(session.PushRegion(tile, 2, 0));
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 1);

  cv::Mat pixels;
  cv::Mat mask;
  std::string reason;
  CHECK(LoadPixels(session.SavedPaths()[0], pixels));
  CHECK(ReadCoverageMask(session.Coverage()[0].mask_path, mask, reason));
  CHECK(pixels.rows == 2);
  CHECK(pixels.cols == 4);
  CHECK(pixels.at<std::uint16_t>(0, 0) == 0);
  CHECK(mask.at<std::uint8_t>(0, 0) == kCoverageUnfilled);
  CHECK(pixels.at<std::uint16_t>(0, 2) == 0);
  CHECK(mask.at<std::uint8_t>(0, 2) == kCoverageValid);
  CHECK(mask.at<std::uint8_t>(1, 3) == kCoverageValid);

  const IntensityStats stats = IntensityOnCoverage(pixels, mask);
  CHECK(stats.available);
  CHECK(stats.min_value == 0);
  CHECK(stats.max_value == 8);
  CHECK(Near(stats.mean, 6));
  CHECK(stats.invalid_count == 4);
  CHECK(stats.valid_count == 4);
  CHECK(session.Coverage()[0].stats.min_value == 0);
  CHECK(Near(session.Coverage()[0].stats.mean, 6));

  std::filesystem::remove_all(dir);
}

/// Короткая последняя полоса записывается. Справа остаются нули, и они не входят в статистику.
void TestShortStripIsSaved() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_coverage_short";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  const std::string chosen = (dir / "short.dcm").string();
  ScanSession session(chosen, Geometry());
  cv::Mat full(40, 20, CV_16UC1, cv::Scalar(1000));
  cv::Mat narrow(30, 8, CV_16UC1, cv::Scalar(2000));
  CHECK(session.PushRegion(full, 0, 0));
  CHECK(session.PushRegion(narrow, 0, 25));
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 1);

  cv::Mat pixels;
  cv::Mat mask;
  std::string reason;
  const std::string path = MakeDicomSegmentPath(chosen, 1);
  CHECK(LoadPixels(path, pixels));
  CHECK(ReadCoverageMask(CoverageMaskPath(path), mask, reason));
  CHECK(pixels.rows == 55);
  CHECK(pixels.cols == 20);
  CHECK(pixels.at<std::uint16_t>(39, 0) == 1000);
  CHECK(pixels.at<std::uint16_t>(40, 0) == 2000);
  CHECK(pixels.at<std::uint16_t>(54, 7) == 2000);
  CHECK(pixels.at<std::uint16_t>(40, 8) == 0);
  CHECK(mask.at<std::uint8_t>(40, 0) == kCoverageValid);
  CHECK(mask.at<std::uint8_t>(40, 8) == kCoverageUnfilled);
  CHECK(mask.at<std::uint8_t>(54, 19) == kCoverageUnfilled);
  CHECK(reason == kCoverageGapUnfilled);

  double raw_min = 0;
  cv::minMaxLoc(pixels, &raw_min, nullptr);
  CHECK(raw_min == 0);
  const IntensityStats stats = IntensityOnCoverage(pixels, mask);
  CHECK(stats.available);
  CHECK(stats.min_value == 1000);
  CHECK(stats.max_value == 2000);
  CHECK(stats.invalid_count == 15 * 12);
  CHECK(stats.valid_count == 55 * 20 - 15 * 12);
  CHECK(Near(stats.mean, (40.0 * 20.0 * 1000.0 + 15.0 * 8.0 * 2000.0) / stats.valid_count));

  std::filesystem::remove_all(dir);
}

/// Разрез сегмента уносит маску хвоста. Дыры хвоста не становятся измеренными нулями.
void TestSplitKeepsTailMask() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_coverage_split_ctest";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  const std::string chosen = (dir / "roll.dcm").string();
  ScanSession session(chosen, Geometry(), 50);
  cv::Mat wide(40, 16, CV_16UC1, cv::Scalar(1000));
  cv::Mat narrow(30, 6, CV_16UC1, cv::Scalar(2000));
  CHECK(session.PushRegion(wide, 0, 0));
  CHECK(session.PushRegion(narrow, 0, 30));
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 2);
  CHECK(session.Coverage().size() == 2);

  CHECK(session.Coverage()[0].complete);
  CHECK(session.Coverage()[0].gap_reason.empty());
  CHECK(session.Coverage()[0].rows == 40);
  CHECK(session.Coverage()[0].stats.min_value == 1000);
  CHECK(session.Coverage()[0].stats.max_value == 1000);
  CHECK(session.Coverage()[0].stats.invalid_count == 0);

  cv::Mat head_pixels;
  cv::Mat head_mask;
  std::string head_reason;
  CHECK(LoadPixels(session.SavedPaths()[0], head_pixels));
  CHECK(ReadCoverageMask(session.Coverage()[0].mask_path, head_mask, head_reason));
  CHECK(head_reason.empty());
  CHECK(head_pixels.rows == 40);
  CHECK(head_pixels.at<std::uint16_t>(0, 0) == 1000);
  CHECK(cv::countNonZero(head_mask == 0) == 0);

  CHECK(!session.Coverage()[1].complete);
  CHECK(session.Coverage()[1].gap_reason == kCoverageGapUnfilled);
  CHECK(session.Coverage()[1].rows == 20);
  CHECK(session.Coverage()[1].cols == 16);
  CHECK(session.Coverage()[1].stats.available);
  CHECK(session.Coverage()[1].stats.min_value == 2000);
  CHECK(session.Coverage()[1].stats.max_value == 2000);
  CHECK(session.Coverage()[1].stats.invalid_count == 20 * 10);
  CHECK(session.Coverage()[1].stats.valid_count == 20 * 6);
  CHECK(Near(session.Coverage()[1].stats.mean, 2000));

  cv::Mat tail_pixels;
  cv::Mat tail_mask;
  std::string tail_reason;
  CHECK(LoadPixels(session.SavedPaths()[1], tail_pixels));
  CHECK(ReadCoverageMask(session.Coverage()[1].mask_path, tail_mask, tail_reason));
  CHECK(tail_reason == kCoverageGapUnfilled);
  CHECK(tail_pixels.rows == 20);
  CHECK(tail_pixels.cols == 16);
  CHECK(tail_pixels.at<std::uint16_t>(0, 0) == 2000);
  CHECK(tail_mask.at<std::uint8_t>(0, 0) == kCoverageValid);
  CHECK(tail_pixels.at<std::uint16_t>(0, 5) == 2000);
  CHECK(tail_mask.at<std::uint8_t>(0, 5) == kCoverageValid);
  CHECK(tail_pixels.at<std::uint16_t>(19, 6) == 0);
  CHECK(tail_mask.at<std::uint8_t>(19, 6) == kCoverageUnfilled);
  CHECK(tail_mask.at<std::uint8_t>(19, 15) == kCoverageUnfilled);

  double raw_min = 0;
  cv::minMaxLoc(tail_pixels, &raw_min, nullptr);
  CHECK(raw_min == 0);
  const IntensityStats tail_stats = IntensityOnCoverage(tail_pixels, tail_mask);
  CHECK(tail_stats.min_value == 2000);
  CHECK(tail_stats.max_value == 2000);
  CHECK(Near(tail_stats.mean, 2000));

  std::filesystem::remove_all(dir);
}

} // namespace

int main() {
  TestUnfilledSidecarSkippedByStats();
  TestMeasuredZeroCounts();
  TestShortStripIsSaved();
  TestSplitKeepsTailMask();

  if (g_failures != 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "test_coverage ok\n";
  return 0;
}

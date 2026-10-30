#include <tape_scanner/scanner/DicomNumbers.hpp>
#include <tape_scanner/scanner/DicomSeries.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>
#include <tape_scanner/scanner/Utils.hpp>
#include <tape_scanner/scanner/WindowLevel.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <opencv2/core.hpp>

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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
bool NearMm(double a, double b) { return std::abs(a - b) < 1e-4; }

struct DicomTags {
  std::string study;
  std::string series;
  std::string sop;
  std::string sop_class;
  std::string modality;
  std::string conversion;
  std::string photometric;
  std::string instance;
  double spacing_row = 0;
  double spacing_col = 0;
  unsigned rows = 0;
  unsigned cols = 0;
  unsigned bits_allocated = 0;
  unsigned bits_stored = 0;
  unsigned high_bit = 0;
  unsigned pixel_representation = 0;
  double position_row = 0;
  std::string position_x;
  std::string position_z;
};

bool LoadDicom(const std::string &path, DicomTags &tags) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str()).bad()) {
    return false;
  }
  DcmDataset *dataset = file.getDataset();
  auto read_string = [&](const DcmTagKey &tag, std::string &out, unsigned long pos = 0) {
    OFString value;
    if (dataset->findAndGetOFString(tag, value, pos).bad()) {
      return false;
    }
    out = value.c_str();
    return true;
  };
  auto read_u16 = [&](const DcmTagKey &tag, unsigned &out) {
    Uint16 value = 0;
    if (dataset->findAndGetUint16(tag, value).bad()) {
      return false;
    }
    out = value;
    return true;
  };

  std::string row;
  std::string col;
  std::string position_y;
  const bool ok = read_string(DCM_StudyInstanceUID, tags.study) && read_string(DCM_SeriesInstanceUID, tags.series) &&
                  read_string(DCM_SOPInstanceUID, tags.sop) && read_string(DCM_SOPClassUID, tags.sop_class) &&
                  read_string(DCM_Modality, tags.modality) && read_string(DCM_ConversionType, tags.conversion) &&
                  read_string(DCM_PhotometricInterpretation, tags.photometric) &&
                  read_string(DCM_InstanceNumber, tags.instance) && read_string(DCM_PixelSpacing, row, 0) &&
                  read_string(DCM_PixelSpacing, col, 1) && read_string(DCM_ImagePositionPatient, tags.position_x, 0) &&
                  read_string(DCM_ImagePositionPatient, position_y, 1) &&
                  read_string(DCM_ImagePositionPatient, tags.position_z, 2) && read_u16(DCM_Rows, tags.rows) &&
                  read_u16(DCM_Columns, tags.cols) && read_u16(DCM_BitsAllocated, tags.bits_allocated) &&
                  read_u16(DCM_BitsStored, tags.bits_stored) && read_u16(DCM_HighBit, tags.high_bit) &&
                  read_u16(DCM_PixelRepresentation, tags.pixel_representation);
  if (!ok) {
    return false;
  }
  tags.spacing_row = ParseDicomDecimal(row).value_or(0);
  tags.spacing_col = ParseDicomDecimal(col).value_or(0);
  tags.position_row = ParseDicomDecimal(position_y).value_or(0);
  return true;
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
  std::memcpy(out.data, data, count * sizeof(Uint16));
  return true;
}

uint16_t BlendPixel(uint16_t previous, uint16_t incoming, int index, int count) {
  cv::Mat sample(1, 1, CV_32FC1);
  const float alpha = 1.f - static_cast<float>(index) / static_cast<float>(count);
  sample.at<float>(0, 0) = static_cast<float>(previous) * alpha + static_cast<float>(incoming) * (1.f - alpha);
  cv::Mat rounded;
  sample.convertTo(rounded, CV_16UC1);
  return rounded.at<uint16_t>(0, 0);
}

int CountValue(const cv::Mat &img, uint16_t value) {
  int count = 0;
  for (int row = 0; row < img.rows; ++row) {
    for (int col = 0; col < img.cols; ++col) {
      if (img.at<uint16_t>(row, col) == value) {
        ++count;
      }
    }
  }
  return count;
}

void CheckSecondaryCapture(const DicomTags &tags, const std::string &study, const std::string &series,
                           const std::string &instance, double row_mm, double col_mm) {
  CHECK(tags.study == study);
  CHECK(tags.series == series);
  CHECK(tags.sop_class == UID_SecondaryCaptureImageStorage);
  CHECK(tags.modality == "OT");
  CHECK(tags.conversion == "DF");
  CHECK(tags.photometric == "MONOCHROME2");
  CHECK(tags.instance == instance);
  CHECK(tags.bits_allocated == 16);
  CHECK(tags.bits_stored == 16);
  CHECK(tags.high_bit == 15);
  CHECK(tags.pixel_representation == 0);
  CHECK(tags.rows <= static_cast<unsigned>(kMaxDicomDimension));
  CHECK(tags.cols <= static_cast<unsigned>(kMaxDicomDimension));
  CHECK(Near(tags.spacing_row, row_mm));
  CHECK(Near(tags.spacing_col, col_mm));
}

void TestBinningAndSpacing() {
  cv::Mat src(2, 4, CV_16UC1);
  src.at<uint16_t>(0, 0) = 1;
  src.at<uint16_t>(0, 1) = 1;
  src.at<uint16_t>(1, 0) = 0;
  src.at<uint16_t>(1, 1) = 0;
  src.at<uint16_t>(0, 2) = 0;
  src.at<uint16_t>(0, 3) = 2;
  src.at<uint16_t>(1, 2) = 0;
  src.at<uint16_t>(1, 3) = 4;

  cv::Mat binned = ApplyScanBinning(src, 2);
  CHECK(binned.type() == CV_16UC1);
  CHECK(binned.rows == 1);
  CHECK(binned.cols == 2);
  // (1+1+0+0+2)/4 = 1; (0+2+0+4+2)/4 = 2. Половина округляется вверх.
  CHECK(binned.at<uint16_t>(0, 0) == 1);
  CHECK(binned.at<uint16_t>(0, 1) == 2);

  cv::Mat saturated(2, 2, CV_16UC1, cv::Scalar(65535));
  cv::Mat saturated_bin = ApplyScanBinning(saturated, 2);
  CHECK(saturated_bin.at<uint16_t>(0, 0) == 65535);

  // Не прореживание: левый столбец нулевой, правый 100. Среднее 50, а не 0.
  cv::Mat sparse(2, 2, CV_16UC1);
  sparse.at<uint16_t>(0, 0) = 0;
  sparse.at<uint16_t>(0, 1) = 100;
  sparse.at<uint16_t>(1, 0) = 0;
  sparse.at<uint16_t>(1, 1) = 100;
  CHECK(ApplyScanBinning(sparse, 2).at<uint16_t>(0, 0) == 50);

  cv::Mat odd(3, 3, CV_16UC1, cv::Scalar(9));
  odd.at<uint16_t>(0, 0) = 1;
  odd.at<uint16_t>(0, 1) = 2;
  odd.at<uint16_t>(1, 0) = 3;
  odd.at<uint16_t>(1, 1) = 4;
  cv::Mat odd_bin = ApplyScanBinning(odd, 2);
  CHECK(odd_bin.rows == 1);
  CHECK(odd_bin.cols == 1);
  CHECK(odd_bin.at<uint16_t>(0, 0) == 3);

  cv::Mat same = ApplyScanBinning(src, 1);
  CHECK(same.rows == src.rows);
  CHECK(same.cols == src.cols);
  CHECK(same.at<uint16_t>(0, 0) == 1);

  ScanGeometry fine;
  fine.bin = 1;
  fine.pixel_um_x = 21.0;
  fine.pixel_um_y = 21.0;
  ScanGeometry coarse = fine;
  coarse.bin = 2;
  CHECK(Near(fine.RowSpacingMm(), 0.021));
  CHECK(Near(fine.ColSpacingMm(), 0.021));
  CHECK(Near(coarse.RowSpacingMm(), 2.0 * fine.RowSpacingMm()));
  CHECK(Near(coarse.ColSpacingMm(), 2.0 * fine.ColSpacingMm()));
  CHECK(Near(coarse.RowSpacingMm(), 0.042));
  CHECK(FormatPitchUm(21.4, 2) == "42.8");
  CHECK(FormatPitchUm(21.0, 1) == "21");

  ScanGeometry measured;
  measured.bin = 2;
  measured.pixel_um_x = 21.4;
  measured.pixel_um_y = 21.4;
  CHECK(Near(measured.RowSpacingMm(), 0.0428));
  CHECK(Near(measured.ColSpacingMm(), 0.0428));

  ScanGeometry axes;
  axes.bin = 2;
  axes.pixel_um_x = 21.0;
  axes.pixel_um_y = 25.0;
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_binning";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "spacing.dcm").string();
  cv::Mat sample(2, 2, CV_16UC1, cv::Scalar(7));
  DicomWriteParams params;
  params.pixel_spacing_row_mm = axes.RowSpacingMm();
  params.pixel_spacing_col_mm = axes.ColSpacingMm();
  params.instance_number = 1;
  CHECK(saveToDcm(sample, path, params));
  DicomTags tags;
  CHECK(LoadDicom(path, tags));
  CHECK(Near(tags.spacing_row, 0.050));
  CHECK(Near(tags.spacing_col, 0.042));
  CHECK(tags.position_x == "0");
  CHECK(NearMm(tags.position_row, 0.0));
  CHECK(tags.position_z == "0");

  const std::string measured_path = (dir / "measured.dcm").string();
  DicomWriteParams measured_params;
  measured_params.pixel_spacing_row_mm = measured.RowSpacingMm();
  measured_params.pixel_spacing_col_mm = measured.ColSpacingMm();
  measured_params.instance_number = 1;
  CHECK(saveToDcm(sample, measured_path, measured_params));
  DicomTags measured_tags;
  CHECK(LoadDicom(measured_path, measured_tags));
  CHECK(Near(measured_tags.spacing_row, 0.0428));
  CHECK(Near(measured_tags.spacing_col, 0.0428));
  std::filesystem::remove_all(dir);
}

void TestSegmentSplit() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_segments";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string chosen = (dir / "roll.dcm").string();

  ScanGeometry geometry;
  geometry.bin = 2;
  geometry.pixel_um_x = kNominalPixelUm;
  geometry.pixel_um_y = kNominalPixelUm;

  ScanSession empty_session(chosen, geometry);
  CHECK(empty_session.Finish());
  CHECK(empty_session.SavedSegmentCount() == 0);
  CHECK(!std::filesystem::exists(MakeDicomSegmentPath(chosen, 1)));

  ScanSession session(chosen, geometry);
  cv::Mat head(kMaxDicomDimension, 4, CV_16UC1, cv::Scalar(1000));
  cv::Mat tail(100, 4, CV_16UC1, cv::Scalar(2000));
  CHECK(session.PushRegion(head, 0, 0));
  CHECK(session.SavedSegmentCount() == 0);
  CHECK(session.PushRegion(tail, 0, kMaxDicomDimension));
  CHECK(session.SavedSegmentCount() == 1);
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 2);

  const std::string path1 = MakeDicomSegmentPath(chosen, 1);
  const std::string path2 = MakeDicomSegmentPath(chosen, 2);
  CHECK(session.SavedPaths().size() == 2);
  CHECK(session.SavedPaths()[0] == path1);
  CHECK(session.SavedPaths()[1] == path2);

  const auto size_before = std::filesystem::file_size(path1);
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 2);
  CHECK(std::filesystem::file_size(path1) == size_before);
  CHECK(!std::filesystem::exists(MakeDicomSegmentPath(chosen, 3)));

  DicomTags first;
  DicomTags second;
  CHECK(LoadDicom(path1, first));
  CHECK(LoadDicom(path2, second));
  CheckSecondaryCapture(first, session.StudyUid(), session.SeriesUid(), "1", geometry.RowSpacingMm(),
                        geometry.ColSpacingMm());
  CheckSecondaryCapture(second, session.StudyUid(), session.SeriesUid(), "2", geometry.RowSpacingMm(),
                        geometry.ColSpacingMm());
  CHECK(first.rows == static_cast<unsigned>(kMaxDicomDimension));
  CHECK(second.rows == 100);
  CHECK(first.cols == 4);
  CHECK(second.cols == 4);
  CHECK(first.sop != second.sop);
  CHECK(first.study == second.study);
  CHECK(first.series == second.series);
  CHECK(first.position_x == "0");
  CHECK(first.position_z == "0");
  CHECK(second.position_x == "0");
  CHECK(second.position_z == "0");
  CHECK(NearMm(first.position_row, 0.0));
  CHECK(NearMm(second.position_row, static_cast<double>(first.rows) * geometry.RowSpacingMm()));

  std::filesystem::remove_all(dir);
}

void TestOverlapCutAndShortStripe() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_overlap";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  ScanGeometry geometry;
  geometry.bin = 2;
  geometry.pixel_um_x = kNominalPixelUm;
  geometry.pixel_um_y = kNominalPixelUm;

  const std::string chosen = (dir / "roll.dcm").string();
  ScanSession session(chosen, geometry, 100);

  cv::Mat head(80, 4, CV_16UC1, cv::Scalar(1000));
  cv::Mat next(80, 4, CV_16UC1, cv::Scalar(2000));
  next.rowRange(0, 30).setTo(3000);
  CHECK(session.PushRegion(head, 0, 0));
  CHECK(session.PushRegion(head.clone(), 4, 0));
  CHECK(session.PushRegion(next, 0, 50));
  CHECK(session.PushRegion(next.clone(), 4, 50));
  CHECK(session.SavedSegmentCount() == 0);
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 2);

  const std::string path1 = MakeDicomSegmentPath(chosen, 1);
  const std::string path2 = MakeDicomSegmentPath(chosen, 2);
  cv::Mat pixels1;
  cv::Mat pixels2;
  DicomTags tags1;
  DicomTags tags2;
  CHECK(LoadPixels(path1, pixels1));
  CHECK(LoadPixels(path2, pixels2));
  CHECK(LoadDicom(path1, tags1));
  CHECK(LoadDicom(path2, tags2));
  CHECK(pixels1.rows == 80);
  CHECK(pixels1.cols == 8);
  CHECK(pixels2.rows == 50);
  CHECK(pixels1.rows + pixels2.rows == 130);
  CHECK(pixels1.at<uint16_t>(49, 0) == 1000);
  CHECK(pixels1.at<uint16_t>(49, 7) == 1000);
  CHECK(pixels1.at<uint16_t>(50, 0) == 1000);
  const uint16_t seam = BlendPixel(1000, 3000, 29, 30);
  CHECK(seam != 1000);
  CHECK(pixels1.at<uint16_t>(79, 0) == seam);
  CHECK(pixels1.at<uint16_t>(79, 7) == seam);
  CHECK(CountValue(pixels2, 2000) == 50 * 8);
  CHECK(CountValue(pixels2, 3000) == 0);
  CHECK(NearMm(tags1.position_row, 0.0));
  CHECK(NearMm(tags2.position_row, static_cast<double>(pixels1.rows) * geometry.RowSpacingMm()));
  CHECK(tags1.study == tags2.study);
  CHECK(tags1.series == tags2.series);
  CHECK(tags1.instance == "1");
  CHECK(tags2.instance == "2");

  const std::string short_path = (dir / "short.dcm").string();
  ScanSession short_session(short_path, geometry);
  cv::Mat full(40, 20, CV_16UC1, cv::Scalar(1000));
  cv::Mat narrow(30, 8, CV_16UC1, cv::Scalar(2000));
  CHECK(short_session.PushRegion(full, 0, 0));
  CHECK(short_session.PushRegion(narrow, 0, 25));
  CHECK(short_session.Finish());
  CHECK(short_session.SavedSegmentCount() == 1);
  cv::Mat short_pixels;
  CHECK(LoadPixels(MakeDicomSegmentPath(short_path, 1), short_pixels));
  CHECK(short_pixels.rows == 55);
  CHECK(short_pixels.cols == 20);
  CHECK(short_pixels.at<uint16_t>(39, 0) == 1000);
  CHECK(short_pixels.at<uint16_t>(40, 0) == 2000);
  CHECK(short_pixels.at<uint16_t>(40, 8) == 0);

  const auto missing = dir / "missing" / "roll.dcm";
  ScanSession retry(missing.string(), geometry);
  cv::Mat wide(20, 16, CV_16UC1, cv::Scalar(1000));
  cv::Mat partial(10, 6, CV_16UC1, cv::Scalar(2000));
  CHECK(retry.PushRegion(wide, 0, 0));
  CHECK(retry.PushRegion(partial, 0, 20));
  CHECK(!retry.Finish());
  CHECK(retry.SavedSegmentCount() == 0);
  CHECK(!std::filesystem::exists(MakeDicomSegmentPath(missing.string(), 1)));
  std::filesystem::create_directories(missing.parent_path());
  CHECK(retry.Finish());
  CHECK(retry.SavedSegmentCount() == 1);
  cv::Mat retried;
  CHECK(LoadPixels(MakeDicomSegmentPath(missing.string(), 1), retried));
  CHECK(retried.rows == 30);
  CHECK(retried.cols == 16);
  CHECK(retried.at<uint16_t>(0, 0) == 1000);
  CHECK(retried.at<uint16_t>(20, 0) == 2000);
  CHECK(retried.at<uint16_t>(20, 6) == 0);
  const auto saved_size = std::filesystem::file_size(MakeDicomSegmentPath(missing.string(), 1));
  CHECK(retry.Finish());
  CHECK(retry.SavedSegmentCount() == 1);
  CHECK(std::filesystem::file_size(MakeDicomSegmentPath(missing.string(), 1)) == saved_size);
  CHECK(!std::filesystem::exists(MakeDicomSegmentPath(missing.string(), 2)));

  const std::vector<DicomSeriesPart> series = ListDicomSeries(path1);
  CHECK(series.size() == 2);
  CHECK(series[0].rows == 80);
  CHECK(series[1].rows == 50);
  CHECK(series[0].instance == 1);
  CHECK(series[1].instance == 2);
  CHECK(NearMm(series[0].position_row_mm, 0.0));
  CHECK(NearMm(series[1].position_row_mm,
               series[0].position_row_mm + static_cast<double>(series[0].rows) * series[0].spacing_row_mm));

  std::filesystem::remove_all(dir);
}

void TestSeriesListSkipsPixelData() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_series_header";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  cv::Mat frame(128, 128, CV_16UC1, cv::Scalar(9));
  CHECK(static_cast<unsigned>(frame.rows * frame.cols * static_cast<int>(sizeof(uint16_t))) > kDicomSeriesValueLimit);

  const std::string chosen = (dir / "roll.dcm").string();
  DicomWriteParams first;
  first.study_instance_uid = "1.2.3.4.5.6.7.800.1";
  first.series_instance_uid = "1.2.3.4.5.6.7.800.2";
  first.instance_number = 1;
  first.pixel_spacing_row_mm = 0.042;
  first.pixel_spacing_col_mm = 0.042;
  first.image_position_row_mm = 0;
  DicomWriteParams second = first;
  second.instance_number = 2;
  second.image_position_row_mm = 128 * 0.042;
  DicomWriteParams stranger = first;
  stranger.series_instance_uid = "1.2.3.4.5.6.7.800.9";
  stranger.instance_number = 1;
  stranger.image_position_row_mm = 0;

  const std::string path1 = MakeDicomSegmentPath(chosen, 1);
  const std::string path2 = MakeDicomSegmentPath(chosen, 2);
  const std::string other = (dir / "other_series.dcm").string();
  CHECK(saveToDcm(frame, path1, first));
  CHECK(saveToDcm(frame, path2, second));
  CHECK(saveToDcm(frame, other, stranger));

  const std::vector<DicomSeriesPart> series = ListDicomSeries(path2);
  CHECK(series.size() == 2);
  CHECK(series[0].instance == 1);
  CHECK(series[1].instance == 2);
  CHECK(series[0].rows == 128);
  CHECK(series[1].cols == 128);
  CHECK(NearMm(series[0].position_row_mm, 0.0));
  CHECK(NearMm(series[1].position_row_mm, 128 * 0.042));
  CHECK(Near(series[1].spacing_row_mm, 0.042));

  DcmFileFormat header;
  CHECK(header.loadFile(path1.c_str(), EXS_Unknown, EGL_noChange, static_cast<Uint32>(kDicomSeriesValueLimit)).good());
  DcmElement *pixels = nullptr;
  CHECK(header.getDataset()->findAndGetElement(DCM_PixelData, pixels).good());
  CHECK(pixels != nullptr);
  CHECK(!pixels->valueLoaded());

  std::filesystem::remove_all(dir);
}

void TestWindowLevel() {
  const WindowLevel window{1000, 200};
  CHECK(Near(DisplayGray(1000, window, false), 0.5));
  CHECK(DisplayGray(0, window, false) == 0);
  CHECK(DisplayGray(2000, window, false) == 1);
  CHECK(DisplayGray(0, window, true) == 1);
  CHECK(DisplayGray(2000, window, true) == 0);
  CHECK(std::abs(ShaderDisplay(1000, 1000, 200, false) - 0.5f) < 1e-4f);
  CHECK(ShaderDisplay(0, 1000, 200, false) == 0.f);
  CHECK(ShaderDisplay(2000, 1000, 200, false) == 1.f);

  cv::Mat sample(2, 2, CV_16UC1);
  sample.at<uint16_t>(0, 0) = 0;
  sample.at<uint16_t>(0, 1) = 1000;
  sample.at<uint16_t>(1, 0) = 2000;
  sample.at<uint16_t>(1, 1) = 4000;
  const auto [min_value, max_value] = RawMinMax(sample);
  const WindowLevel fitted = WindowFromMinMax(min_value, max_value);
  CHECK(min_value == 0);
  CHECK(max_value == 4000);
  CHECK(Near(fitted.center, 2000));
  CHECK(Near(fitted.width, 4000));

  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_window";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "frame.dcm").string();
  DicomWriteParams params;
  params.instance_number = 1;
  CHECK(saveToDcm(sample, path, params));
  cv::Mat stored = ReadStoredMono16(path);
  CHECK(stored.rows == 2);
  CHECK(stored.at<uint16_t>(0, 1) == 1000);
  CHECK(stored.at<uint16_t>(1, 0) == 2000);
  DisplayGray(stored.at<uint16_t>(0, 1), window, true);
  cv::Mat again = ReadStoredMono16(path);
  CHECK(again.at<uint16_t>(0, 1) == 1000);

  cv::Mat grid(2, 2, CV_16UC1);
  grid.at<uint16_t>(0, 0) = 1;
  grid.at<uint16_t>(0, 1) = 2;
  grid.at<uint16_t>(1, 0) = 3;
  grid.at<uint16_t>(1, 1) = 4;
  cv::Mat turned = PresentFrame(grid, 90, 0);
  CHECK(turned.at<uint16_t>(0, 0) == 3);
  CHECK(turned.at<uint16_t>(0, 1) == 1);
  CHECK(turned.at<uint16_t>(1, 1) == 2);
  cv::Mat flipped = PresentFrame(grid, 0, 1);
  CHECK(flipped.at<uint16_t>(0, 0) == 2);
  CHECK(flipped.at<uint16_t>(1, 1) == 3);

  WindowLevel dragged{1000, 200};
  DragWindow(dragged, 0, -100, 100, 100);
  CHECK(Near(dragged.center, kDisplayMaxValue));
  CHECK(Near(dragged.width, 200));
  DragWindow(dragged, 100, 0, 100, 100);
  CHECK(Near(dragged.width, kDisplayMaxValue));
  CHECK(UseNearestFilter(1.f));
  CHECK(UseNearestFilter(2.f));
  CHECK(!UseNearestFilter(0.5f));

  std::filesystem::remove_all(dir);
}

void TestRulerPitch() {
  double pitch = kNominalPixelUm;
  CHECK(!PitchUmFromRuler(0.0, 476.0, pitch));
  CHECK(!PitchUmFromRuler(10.0, 0.0, pitch));
  CHECK(!PitchUmFromRuler(-1.0, 476.0, pitch));
  CHECK(!OpticalPitchFromSavedGrid(10.0, 238.0, 3, pitch));
  CHECK(pitch == kNominalPixelUm);

  // 10 мм на 476 оптических пикселях. 27/1280 в номинал не подставляется.
  CHECK(PitchUmFromRuler(10.0, 476.0, pitch));
  CHECK(Near(pitch, 10.0 / 476.0 * 1000.0));
  CHECK(kNominalPixelUm == 21.0);
  double from_field = 0;
  CHECK(PitchUmFromRuler(27.0, 1280.0, from_field));
  CHECK(Near(from_field, 27.0 / 1280.0 * 1000.0));
  CHECK(kNominalPixelUm == 21.0);

  double optical_x = 0;
  CHECK(OpticalPitchFromSavedGrid(10.0, 238.0, 2, optical_x));
  CHECK(Near(optical_x, 10.0 / (238.0 * 2.0) * 1000.0));
  CHECK(Near(optical_x, pitch));
  ScanGeometry measured;
  measured.bin = 2;
  measured.pixel_um_x = optical_x;
  measured.pixel_um_y = kNominalPixelUm;
  CHECK(Near(measured.ColSpacingMm(), 10.0 / 238.0));
  CHECK(Near(measured.RowSpacingMm(), 21.0 * 2.0 / 1000.0));
  CHECK(FormatPitchUm(optical_x, 1) == "21.0084");
  CHECK(FormatPitchUm(optical_x, 2) == "42.0168");

  double optical_y = 0;
  CHECK(OpticalPitchFromSavedGrid(20.0, 400.0, 2, optical_y));
  CHECK(Near(optical_y, 25.0));
  measured.pixel_um_y = optical_y;
  CHECK(Near(measured.RowSpacingMm(), 20.0 / 400.0));
  CHECK(Near(measured.ColSpacingMm(), 10.0 / 238.0));

  double px = 0;
  double py = 0;
  CHECK(ImagePixelFromModel(-1.0, 1.0, 20, 30, px, py));
  CHECK(Near(px, 0.0));
  CHECK(Near(py, 0.0));
  CHECK(ImagePixelFromModel(1.0, -1.0, 20, 30, px, py));
  CHECK(Near(px, 20.0));
  CHECK(Near(py, 30.0));
  CHECK(!ImagePixelFromModel(0, 0, 0, 10, px, py));

  double sx = 0;
  double sy = 0;
  CHECK(SourceMarkFromPresented(29.0, 0.0, 20, 30, 90, 0, sx, sy));
  CHECK(Near(sx, 0.0));
  CHECK(Near(sy, 0.0));
  double sx2 = 0;
  double sy2 = 0;
  CHECK(SourceMarkFromPresented(29.0, 10.0, 20, 30, 90, 0, sx2, sy2));
  CHECK(Near(sx2, 10.0));
  CHECK(Near(sy2, 0.0));
  CHECK(Near(AxisPixelSpan(sx, sy, sx2, sy2, true), 10.0));
  CHECK(Near(AxisPixelSpan(sx, sy, sx2, sy2, false), 0.0));

  CHECK(SourceMarkFromPresented(19.0, 5.0, 20, 30, 0, 1, sx, sy));
  CHECK(SourceMarkFromPresented(9.0, 5.0, 20, 30, 0, 1, sx2, sy2));
  CHECK(Near(AxisPixelSpan(sx, sy, sx2, sy2, true), 10.0));

  CHECK(SourceMarkFromPresented(0.0, 0.0, 2, 2, 90, 0, sx, sy));
  CHECK(Near(sx, 0.0));
  CHECK(Near(sy, 1.0));
}

} // namespace

int main() {
  // Сканер, камера и окно программы не нужны: проверяются сетка и DICOM-файлы.
  TestBinningAndSpacing();
  TestSegmentSplit();
  TestOverlapCutAndShortStripe();
  TestSeriesListSkipsPixelData();
  TestWindowLevel();
  TestRulerPitch();

  if (g_failures != 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "test_scan_geometry ok\n";
  return 0;
}

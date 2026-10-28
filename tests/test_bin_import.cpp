#include <tape_scanner/dicom_generator/BinToDcmConverter.hpp>
#include <tape_scanner/scanner/DicomSeries.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>
#include <tape_scanner/scanner/Utils.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <opencv2/core.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
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

bool ThrowsRuntime(const std::function<void()> &fn) {
  try {
    fn();
  } catch (const std::runtime_error &) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

void WriteBytes(const std::filesystem::path &path, const void *data, size_t size) {
  std::ofstream raw(path, std::ios::binary);
  raw.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
}

bool ReadSpacing(const std::string &path, const DcmTagKey &tag, double &row, double &col) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str()).bad()) {
    return false;
  }
  OFString value;
  if (file.getDataset()->findAndGetOFString(tag, value, 0).bad()) {
    return false;
  }
  row = std::strtod(value.c_str(), nullptr);
  if (file.getDataset()->findAndGetOFString(tag, value, 1).bad()) {
    return false;
  }
  col = std::strtod(value.c_str(), nullptr);
  return true;
}

bool ReadPixel(const std::string &path, int row, int col, uint16_t &pixel) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str()).bad()) {
    return false;
  }
  const Uint16 *data = nullptr;
  unsigned long count = 0;
  Uint16 rows = 0;
  Uint16 cols = 0;
  DcmDataset *dataset = file.getDataset();
  if (dataset->findAndGetUint16(DCM_Rows, rows).bad() || dataset->findAndGetUint16(DCM_Columns, cols).bad() ||
      dataset->findAndGetUint16Array(DCM_PixelData, data, &count).bad() || data == nullptr) {
    return false;
  }
  if (row < 0 || col < 0 || row >= rows || col >= cols) {
    return false;
  }
  const unsigned long index = static_cast<unsigned long>(row) * cols + static_cast<unsigned long>(col);
  if (index >= count) {
    return false;
  }
  pixel = data[index];
  return true;
}

using tape_scanner::dicom_generator::ConvertBinToDcm;

const auto kLog = [](const std::string &) {};

// Пропуск двух чисел шага не компилируется. Значения по умолчанию 0.042 нет.
template <typename... Args>
constexpr bool CanConvertBin = requires { ConvertBinToDcm(std::declval<Args>()...); };

static_assert(!CanConvertBin<std::string, std::size_t, std::size_t, std::size_t, std::size_t, bool,
                             std::function<void(const std::string &)>>);
static_assert(CanConvertBin<std::string, std::size_t, std::size_t, std::size_t, std::size_t, bool,
                            std::function<void(const std::string &)>, double, double>);

void RequireSpacing(const std::filesystem::path &dir) {
  const auto raw = dir / "space.bin";
  const uint16_t pixels[4] = {1, 2, 3, 4};
  WriteBytes(raw, pixels, sizeof(pixels));
  const auto out = (dir / "space.dcm").string();
  const double row_mm = 0.015;
  const double col_mm = 0.033;
  const auto written = ConvertBinToDcm(raw.string(), 2, 2, 16, 1, false, kLog, row_mm, col_mm, out);
  CHECK(written.size() == 1);
  CHECK(written[0] == out);
  double row = 0;
  double col = 0;
  double imager_row = 0;
  double imager_col = 0;
  CHECK(ReadSpacing(out, DCM_PixelSpacing, row, col));
  CHECK(ReadSpacing(out, DCM_ImagerPixelSpacing, imager_row, imager_col));
  CHECK(Near(row, row_mm));
  CHECK(Near(col, col_mm));
  CHECK(Near(imager_row, row_mm));
  CHECK(Near(imager_col, col_mm));
  CHECK(!Near(row, 0.042));
  CHECK(!Near(col, 0.042));
  CHECK(!ReadImageScalePassport(out).present);

  for (double bad : {0.0, -0.021, std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity()}) {
    const auto rejected = (dir / "bad_spacing.dcm").string();
    std::filesystem::remove(rejected);
    std::vector<std::string> lines;
    bool returned = false;
    CHECK(ThrowsRuntime([&] {
      auto paths = ConvertBinToDcm(raw.string(), 2, 2, 16, 1, false,
                                   [&](const std::string &msg) { lines.push_back(msg); }, bad, 0.021, rejected);
      returned = true;
      (void)paths;
    }));
    CHECK(!returned);
    CHECK(!std::filesystem::exists(rejected));
    for (const std::string &line : lines) {
      CHECK(line.find("Saved") == std::string::npos);
    }
  }
}

void RequireDeclaredSize(const std::filesystem::path &dir) {
  const uint8_t four[4] = {1, 2, 3, 4};
  const auto exact = dir / "exact.bin";
  WriteBytes(exact, four, sizeof(four));
  const auto exact_dcm = (dir / "exact.dcm").string();
  const auto written = ConvertBinToDcm(exact.string(), 2, 2, 8, 1, false, kLog, 0.021, 0.021, exact_dcm);
  CHECK(written.size() == 1);
  uint16_t pixel = 0;
  CHECK(ReadPixel(exact_dcm, 1, 1, pixel));
  CHECK(pixel == 4);

  const auto short_path = dir / "short.bin";
  WriteBytes(short_path, four, 2);
  const auto short_dcm = (dir / "short.dcm").string();
  bool returned = false;
  CHECK(ThrowsRuntime([&] {
    auto paths = ConvertBinToDcm(short_path.string(), 2, 2, 8, 1, false, kLog, 0.021, 0.021, short_dcm);
    returned = true;
    (void)paths;
  }));
  CHECK(!returned);
  CHECK(!std::filesystem::exists(short_dcm));

  const auto long_path = dir / "long.bin";
  const uint8_t five[5] = {1, 2, 3, 4, 5};
  WriteBytes(long_path, five, sizeof(five));
  const auto long_dcm = (dir / "long.dcm").string();
  CHECK(ThrowsRuntime(
      [&] { ConvertBinToDcm(long_path.string(), 2, 2, 8, 1, false, kLog, 0.021, 0.021, long_dcm); }));
  CHECK(!std::filesystem::exists(long_dcm));

  const auto wide = dir / "wide16.bin";
  WriteBytes(wide, four, sizeof(four));
  const auto wide_dcm = (dir / "wide16.dcm").string();
  CHECK(ThrowsRuntime(
      [&] { ConvertBinToDcm(wide.string(), 2, 2, 16, 1, false, kLog, 0.021, 0.021, wide_dcm); }));
  CHECK(!std::filesystem::exists(wide_dcm));
}

void OddAndShortEightBit(const std::filesystem::path &dir) {
  const uint8_t odd[3] = {200, 10, 11};
  const auto odd_path = dir / "odd.bin";
  WriteBytes(odd_path, odd, sizeof(odd));
  const auto odd_dcm = (dir / "odd.dcm").string();
  const auto written = ConvertBinToDcm(odd_path.string(), 3, 1, 8, 1, false, kLog, 0.017, 0.019, odd_dcm);
  CHECK(written.size() == 1);
  uint16_t pixel = 0;
  CHECK(ReadPixel(odd_dcm, 0, 0, pixel));
  CHECK(pixel == 200);
  CHECK(ReadPixel(odd_dcm, 0, 2, pixel));
  CHECK(pixel == 11);

  const uint8_t one_byte = 255;
  const auto one_path = dir / "one.bin";
  WriteBytes(one_path, &one_byte, 1);
  const auto one_dcm = (dir / "one.dcm").string();
  CHECK(ConvertBinToDcm(one_path.string(), 1, 1, 8, 1, false, kLog, 0.021, 0.021, one_dcm).size() == 1);
  CHECK(ReadPixel(one_dcm, 0, 0, pixel));
  CHECK(pixel == 255);

  const auto short_dcm = (dir / "odd_short.dcm").string();
  CHECK(ThrowsRuntime([&] {
    ConvertBinToDcm(odd_path.string(), 3, 1, 8, 2, false, kLog, 0.021, 0.021, short_dcm);
  }));
  CHECK(!std::filesystem::exists(short_dcm));
}

void SuccessOnlyAfterWrite(const std::filesystem::path &dir) {
  const uint8_t pixel = 7;
  const auto raw = dir / "frame.bin";
  WriteBytes(raw, &pixel, 1);

  std::vector<std::string> saved_log;
  const auto first = (dir / "copies.dcm").string();
  const auto written = ConvertBinToDcm(raw.string(), 1, 1, 8, 2, true, [&](const std::string &msg) { saved_log.push_back(msg); },
                                       0.021, 0.021, first);
  CHECK(written.size() == 2);
  CHECK(std::filesystem::exists(written[0]));
  CHECK(std::filesystem::exists(written[1]));
  CHECK(written[1] == (dir / "copies_002.dcm").string());
  int saved_count = 0;
  for (const std::string &line : saved_log) {
    if (line.find("Saved ") == 0) {
      ++saved_count;
    }
  }
  CHECK(saved_count == 2);
  uint16_t value = 0;
  CHECK(ReadPixel(written[0], 0, 0, value));
  CHECK(value == 7);
  CHECK(ReadPixel(written[1], 0, 0, value));
  CHECK(value == 7);

  const auto missing = (dir / "missing" / "out.dcm").string();
  std::vector<std::string> failed_log;
  bool returned = false;
  CHECK(ThrowsRuntime([&] {
    auto paths = ConvertBinToDcm(raw.string(), 1, 1, 8, 1, false,
                                 [&](const std::string &msg) { failed_log.push_back(msg); }, 0.021, 0.021, missing);
    returned = true;
    (void)paths;
  }));
  CHECK(!returned);
  CHECK(!std::filesystem::exists(missing));
  for (const std::string &line : failed_log) {
    CHECK(line.find("Saved") == std::string::npos);
  }
}

void RulerBinFromPassport(const std::filesystem::path &dir) {
  cv::Mat sample(4, 6, CV_16UC1, cv::Scalar(1));

  DicomWriteParams coarse;
  coarse.pixel_spacing_row_mm = 0.042;
  coarse.pixel_spacing_col_mm = 0.042;
  coarse.bin = 2;
  const std::string coarse_path = (dir / "coarse.dcm").string();
  CHECK(saveToDcm(sample, coarse_path, coarse));
  const ImageScalePassport coarse_passport = ReadImageScalePassport(coarse_path);
  CHECK(coarse_passport.present);
  CHECK(coarse_passport.bin == 2);

  const RulerBinChoice from_file = ChooseRulerBin(coarse_passport.bin, 1);
  CHECK(from_file.ok);
  CHECK(from_file.bin == 2);
  CHECK(from_file.source == RulerBinSource::ImagePassport);
  double optical = 0;
  double doubled = 0;
  CHECK(RulerOpticalPitch(10.0, 238.0, from_file.bin, optical));
  CHECK(RulerOpticalPitch(10.0, 238.0, 1, doubled));
  CHECK(Near(doubled, optical * 2.0));
  CHECK(Near(optical, 10.0 / (238.0 * 2.0) * 1000.0));

  DicomWriteParams same_spacing;
  same_spacing.pixel_spacing_row_mm = 0.042;
  same_spacing.pixel_spacing_col_mm = 0.042;
  const std::string bare_path = (dir / "bare.dcm").string();
  CHECK(saveToDcm(sample, bare_path, same_spacing));
  const ImageScalePassport bare = ReadImageScalePassport(bare_path);
  CHECK(!bare.present);
  CHECK(bare.bin == 0);
  CHECK(!ChooseRulerBin(bare.bin, 0).ok);
  const RulerBinChoice typed = ChooseRulerBin(bare.bin, 2);
  CHECK(typed.ok);
  CHECK(typed.bin == 2);
  CHECK(typed.source == RulerBinSource::ExplicitUser);

  DicomWriteParams fine;
  fine.pixel_spacing_row_mm = 0.042;
  fine.pixel_spacing_col_mm = 0.042;
  fine.bin = 1;
  const std::string fine_path = (dir / "fine.dcm").string();
  CHECK(saveToDcm(sample, fine_path, fine));
  CHECK(ChooseRulerBin(ReadImageScalePassport(fine_path).bin, 2).bin == 1);

  ScanGeometry geometry;
  geometry.bin = 2;
  geometry.pixel_um_x = 21.0;
  geometry.pixel_um_y = 21.0;
  ScanSession session((dir / "roll.dcm").string(), geometry, 100);
  CHECK(session.PushRegion(sample.clone(), 0, 0));
  CHECK(session.Finish());
  CHECK(!session.SavedPaths().empty());
  CHECK(ReadImageScalePassport(session.SavedPaths().front()).bin == 2);
  CHECK(ChooseRulerBin(ReadImageScalePassport(session.SavedPaths().front()).bin, 1).bin == 2);
}

} // namespace

int main() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_ctest_bin_import";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  RequireSpacing(dir);
  RequireDeclaredSize(dir);
  OddAndShortEightBit(dir);
  SuccessOnlyAfterWrite(dir);
  RulerBinFromPassport(dir);

  std::filesystem::remove_all(dir);
  if (g_failures != 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "test_bin_import ok\n";
  return 0;
}

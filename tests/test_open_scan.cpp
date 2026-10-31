#include <tape_scanner/dicom_generator/BinToDcmConverter.hpp>
#include <tape_scanner/dicom_generator/DcmCreator.hpp>
#include <tape_scanner/scanner/DicomNumbers.hpp>
#include <tape_scanner/scanner/DicomScan.hpp>
#include <tape_scanner/scanner/DicomSeries.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/Utils.hpp>
#include <tape_scanner/scanner/WindowLevel.hpp>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>

#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

std::filesystem::path Sample(const char *name) {
  return std::filesystem::path(TAPE_SCANNER_SOURCE_DIR) / "tests" / "data" / name;
}

bool TagContains(const DicomScan &scan, const std::string &needle) {
  for (const DicomTagText &tag : scan.tags) {
    if (tag.value.find(needle) != std::string::npos || tag.name.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

struct WrittenTags {
  std::string meta_sop;
  std::string data_sop;
  std::string meta_instance;
  std::string data_instance;
  std::string spacing;
  std::string aspect;
  std::string charset;
  std::string patient;
};

bool ReadWritten(const std::string &path, WrittenTags &tags) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str()).bad()) {
    return false;
  }
  auto take = [](DcmItem *item, const DcmTagKey &key, std::string &out) {
    OFString value;
    if (!item || item->findAndGetOFStringArray(key, value).bad()) {
      out.clear();
      return false;
    }
    out = value.c_str();
    return true;
  };
  take(file.getMetaInfo(), DCM_MediaStorageSOPClassUID, tags.meta_sop);
  take(file.getDataset(), DCM_SOPClassUID, tags.data_sop);
  take(file.getMetaInfo(), DCM_MediaStorageSOPInstanceUID, tags.meta_instance);
  take(file.getDataset(), DCM_SOPInstanceUID, tags.data_instance);
  take(file.getDataset(), DCM_PixelSpacing, tags.spacing);
  take(file.getDataset(), DCM_PixelAspectRatio, tags.aspect);
  take(file.getDataset(), DCM_SpecificCharacterSet, tags.charset);
  take(file.getDataset(), DCM_PatientName, tags.patient);
  return !tags.data_sop.empty();
}

void TestDecimalsAndCharset() {
  if (std::setlocale(LC_NUMERIC, "ru_RU.UTF-8") != nullptr) {
    char via_printf[32];
    std::snprintf(via_printf, sizeof(via_printf), "%.6f", 0.042);
    CHECK(std::string(via_printf).find(',') != std::string::npos);
  }
  const std::string formatted = FormatDicomDecimal(0.042);
  CHECK(formatted.find(',') == std::string::npos);
  CHECK(formatted.find('.') != std::string::npos);
  const auto parsed = ParseDicomDecimal(formatted);
  CHECK(parsed.has_value());
  CHECK(parsed && Near(*parsed, 0.042));
  CHECK(ParseDicomDecimal("0,042") && Near(*ParseDicomDecimal("0,042"), 0.042));
  CHECK(ParseDicomDecimal("42,33") && Near(*ParseDicomDecimal("42,33"), 42.33));
  CHECK(ParseDicomDecimal("42.33") && Near(*ParseDicomDecimal("42.33"), 42.33));
  CHECK(Near(PixelSpacingToMm(42.33), 0.04233));
  CHECK(Near(PixelSpacingToMm(0.042), 0.042));
  CHECK(Near(PixelSpacingToMm(1.0), 1.0));
  CHECK(Near(PixelSpacingToMm(1.5), 0.0015));
  CHECK(PixelSpacingToMm(0.0) == 0.0);
  CHECK(FormatPitchUm(21.4, 2) == "42.8");

  const unsigned char scan_raw[] = {0xd1, 0xea, 0xe0, 0xed, 0xe8, 0xf0, 0xee, 0xe2, 0xe0, 0xed, 0xe8, 0xe5,
                                   ' ', 0xb9, '4', '_', '1'};
  const std::string scan_bytes(reinterpret_cast<const char *>(scan_raw), sizeof(scan_raw));
  CHECK(DecodeDicomString(scan_bytes, "") == "Сканирование №4_1");
  const std::string iso_privet("\xbf\xe0\xd8\xd2\xd5\xe2", 6);
  CHECK(DecodeDicomString(iso_privet, "ISO_IR 144") == "Привет");
  CHECK(DecodeDicomString("Иванов", "ISO_IR 192") == "Иванов");
  CHECK(DecodeDicomString("Cafe", "ISO_IR 100") == "Cafe");
}

void TestWriters(const std::filesystem::path &dir) {
  cv::Mat sample(4, 6, CV_16UC1, cv::Scalar(1000));
  sample.at<uint16_t>(1, 2) = 2222;
  DicomWriteParams params;
  params.patient_name_utf8 = "Иванов^Иван";
  params.pixel_spacing_row_mm = 0.042;
  params.pixel_spacing_col_mm = 0.021;
  params.instance_number = 1;
  const std::string path = (dir / "saved.dcm").string();
  CHECK(saveToDcm(sample, path, params));
  WrittenTags tags;
  CHECK(ReadWritten(path, tags));
  CHECK(tags.meta_sop == tags.data_sop);
  CHECK(tags.meta_sop == UID_SecondaryCaptureImageStorage);
  CHECK(tags.meta_instance == tags.data_instance);
  CHECK(tags.charset == "ISO_IR 192");
  CHECK(tags.spacing.find(',') == std::string::npos);
  CHECK(tags.spacing.find('.') != std::string::npos);
  CHECK(tags.patient == "Иванов^Иван");
  DicomScan loaded;
  std::string error;
  CHECK(LoadDicomScan(path, loaded, error));
  CHECK(loaded.patient_name == "Иванов^Иван");
  CHECK(Near(loaded.spacing_row_mm, 0.042));
  CHECK(Near(loaded.spacing_col_mm, 0.021));
  CHECK(loaded.frames.size() == 1);
  CHECK(loaded.frames[0].at<uint16_t>(1, 2) == 2222);
  cv::Mat stored = ReadStoredMono16(path);
  CHECK(stored.at<uint16_t>(1, 2) == 2222);

  const std::string created = (dir / "created.dcm").string();
  tape_scanner::dicom_generator::CreateDcm(created, 8, 4, 16, 2, 3, 20, 1, true);
  WrittenTags created_tags;
  CHECK(ReadWritten(created, created_tags));
  CHECK(created_tags.meta_sop == created_tags.data_sop);
  CHECK(created_tags.meta_sop == UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage);
  CHECK(created_tags.meta_instance == created_tags.data_instance);
  CHECK(created_tags.charset == "ISO_IR 192");
  CHECK(created_tags.aspect == "1\\1");
  CHECK(created_tags.aspect.find('[') == std::string::npos);
  CHECK(created_tags.spacing.find(',') == std::string::npos);
  const auto spacing_row = ParseDicomDecimal(created_tags.spacing.substr(0, created_tags.spacing.find('\\')));
  CHECK(spacing_row && Near(*spacing_row, ScanGeometry{}.RowSpacingMm()));

  std::vector<uint16_t> raw(2 * 3, 77);
  raw[1] = 88;
  const std::string raw_path = (dir / "frame.raw").string();
  {
    std::ofstream output(raw_path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(raw.data()), static_cast<std::streamsize>(raw.size() * sizeof(uint16_t)));
  }
  const std::string converted = (dir / "frame.raw.dcm").string();
  tape_scanner::dicom_generator::ConvertBinToDcm(raw_path, 3, 2, 16, 1, false, [](const std::string &) {}, converted);
  WrittenTags bin_tags;
  CHECK(ReadWritten(converted, bin_tags));
  CHECK(bin_tags.meta_sop == bin_tags.data_sop);
  CHECK(bin_tags.meta_sop == UID_SecondaryCaptureImageStorage);
  CHECK(bin_tags.aspect == "1\\1");
  CHECK(bin_tags.charset == "ISO_IR 192");
  CHECK(bin_tags.spacing.find("1.0\\1.0") == std::string::npos);
  CHECK(bin_tags.spacing.find(',') == std::string::npos);
}

void WriteLegacy(const std::string &path, const std::string &spacing, const std::string &patient,
                 const std::string &charset, const std::string &series, int instance) {
  cv::Mat image(2, 2, CV_16UC1, cv::Scalar(5));
  DcmFileFormat file;
  DcmDataset *dataset = file.getDataset();
  dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
  dataset->putAndInsertString(DCM_SOPInstanceUID, (series + "." + std::to_string(instance)).c_str());
  dataset->putAndInsertString(DCM_StudyInstanceUID, "1.2.826.0.1.3680043.9.9");
  dataset->putAndInsertString(DCM_SeriesInstanceUID, series.c_str());
  dataset->putAndInsertString(DCM_Modality, "OT");
  dataset->putAndInsertString(DCM_InstanceNumber, std::to_string(instance).c_str());
  dataset->putAndInsertString(DCM_ImagePositionPatient, instance == 1 ? "0\\0\\0" : "0\\0,084\\0");
  dataset->putAndInsertString(DCM_PixelSpacing, spacing.c_str());
  if (!charset.empty()) {
    dataset->putAndInsertString(DCM_SpecificCharacterSet, charset.c_str());
  }
  dataset->putAndInsertString(DCM_PatientName, patient.c_str());
  dataset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
  dataset->putAndInsertUint16(DCM_BitsAllocated, 16);
  dataset->putAndInsertUint16(DCM_BitsStored, 16);
  dataset->putAndInsertUint16(DCM_HighBit, 15);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
  dataset->putAndInsertUint16(DCM_Rows, 2);
  dataset->putAndInsertUint16(DCM_Columns, 2);
  dataset->putAndInsertUint16Array(DCM_PixelData, reinterpret_cast<const Uint16 *>(image.data), 4);
  file.getMetaInfo()->putAndInsertString(DCM_MediaStorageSOPClassUID, UID_SecondaryCaptureImageStorage);
  CHECK(file.saveFile(path.c_str(), EXS_LittleEndianExplicit).good());
}

void TestLegacySpacingCharsetAndExtensionless(const std::filesystem::path &dir) {
  const std::string legacy = (dir / "legacy.dcm").string();
  const std::string cp1251_name("\xc8\xe2\xe0\xed\xee\xe2", 6);
  WriteLegacy(legacy, "42,33\\42,33", cp1251_name, "", "1.2.3.4.5.6.7.900", 1);
  DicomScan scan;
  std::string error;
  CHECK(LoadDicomScan(legacy, scan, error));
  CHECK(error.empty());
  CHECK(Near(scan.spacing_row_mm, 0.04233));
  CHECK(Near(scan.spacing_col_mm, 0.04233));
  CHECK(scan.patient_name == "Иванов");

  const std::string bare_a = (dir / "roll_a").string();
  const std::string bare_b = (dir / "roll_b").string();
  WriteLegacy(bare_a, "0.042\\0.042", "A", "", "1.2.3.4.5.6.7.901", 1);
  WriteLegacy(bare_b, "0.042\\0.042", "B", "", "1.2.3.4.5.6.7.901", 2);
  CHECK(FileHasDicomMarker(bare_a));
  const std::vector<DicomSeriesPart> series = ListDicomSeries(bare_b);
  CHECK(series.size() == 2);
  CHECK(series[0].instance == 1);
  CHECK(series[1].instance == 2);
  CHECK(Near(series[0].spacing_row_mm, 0.042));
  CHECK(Near(series[1].position_row_mm, 0.084));
}

void TestTilePlan() {
  const DisplayTilePlan array_scan = PlanDisplayTiles(10965, 1050, 16384);
  CHECK(array_scan.scale == 1.0);
  CHECK(array_scan.tile <= 4096);
  CHECK(array_scan.tile <= 16384);
  CHECK(array_scan.tiles_x * array_scan.tile >= array_scan.upload_cols);
  CHECK(array_scan.tiles_y * array_scan.tile >= array_scan.upload_rows);
  CHECK(array_scan.upload_cols == 10965);
  CHECK(array_scan.upload_rows == 1050);

  const DisplayTilePlan iberis = PlanDisplayTiles(1602, 40968, 8192);
  CHECK(iberis.scale == 1.0);
  CHECK(iberis.tile <= 4096);
  CHECK(iberis.upload_rows == 40968);
  CHECK(iberis.tiles_y >= 10);

  const DisplayTilePlan own = PlanDisplayTiles(27024, 1640, 4096);
  CHECK(own.scale == 1.0);
  CHECK(own.tile <= 4096);
  CHECK(own.tiles_x >= 7);
  CHECK(own.upload_cols == 27024);

  const DisplayTilePlan small_gpu = PlanDisplayTiles(3000, 100, 2048);
  CHECK(small_gpu.scale == 1.0);
  CHECK(small_gpu.tile <= 2048);
  CHECK(small_gpu.tiles_x * small_gpu.tile >= 3000);

  const DisplayTilePlan huge = PlanDisplayTiles(10000, 10000, 64);
  CHECK(huge.scale < 1.0);
  CHECK(huge.tile <= 64);
  CHECK(huge.tiles_x * huge.tiles_y <= 256);
  CHECK(huge.upload_cols < 10000);
}

void TestSamples() {
  const auto array_path = Sample("0-90.DCM");
  const auto siemens_path = Sample("MR-SIEMENS-DICOM-WithOverlays.dcm");
  CHECK(std::filesystem::exists(array_path));
  CHECK(std::filesystem::exists(siemens_path));
  CHECK(FileHasDicomMarker(array_path.string()));

  std::vector<uint8_t> array_bytes;
  {
    std::ifstream input(array_path, std::ios::binary);
    array_bytes.assign(std::istreambuf_iterator<char>(input), {});
  }
  DicomScan raw_array;
  std::string raw_error;
  const bool raw_ok = ParseRawDicom(array_bytes.data(), array_bytes.size(), raw_array, raw_error);
  CHECK(raw_ok);
  if (!raw_ok) {
    std::cerr << raw_error << "\n";
  } else if (raw_array.frames.size() == 1 && raw_array.frames[0].type() == CV_16UC1) {
    CHECK(raw_array.rows == 1050);
    CHECK(raw_array.cols == 10965);
    CHECK(raw_array.frames[0].at<uint16_t>(0, 0) == 16275);
    CHECK(TagContains(raw_array, "Сканирование"));
  } else {
    CHECK(false);
  }

  DicomScan array_scan;
  std::string error;
  const bool array_ok = LoadDicomScan(array_path.string(), array_scan, error);
  CHECK(array_ok);
  if (!array_ok) {
    std::cerr << error << "\n";
  } else if (array_scan.frames.size() == 1 && !array_scan.frames[0].empty() && array_scan.frames[0].type() == CV_16UC1) {
    CHECK(array_scan.rows == 1050);
    CHECK(array_scan.cols == 10965);
    CHECK(array_scan.bits_allocated == 16);
    CHECK(array_scan.bits_stored == 16);
    CHECK(array_scan.photometric == "MONOCHROME2");
    CHECK(array_scan.modality == "CR");
    CHECK(array_scan.sop_class_uid == UID_ComputedRadiographyImageStorage);
    CHECK(Near(array_scan.spacing_row_mm, 0.099964));
    CHECK(Near(array_scan.spacing_col_mm, 0.100095));
    CHECK(array_scan.frames[0].rows == 1050);
    CHECK(array_scan.frames[0].cols == 10965);
    CHECK(array_scan.frames[0].at<uint16_t>(0, 0) == 16275);
    double min_value = 0;
    double max_value = 0;
    cv::minMaxLoc(array_scan.frames[0], &min_value, &max_value);
    CHECK(min_value == 0);
    CHECK(max_value == 16275);
    CHECK(TagContains(array_scan, "Сканирование"));
    CHECK(TagContains(array_scan, "ARRAY SCANNER"));
    cv::Mat stored = ReadStoredMono16(array_path.string());
    CHECK(!stored.empty());
    if (!stored.empty() && stored.type() == CV_16UC1) {
      CHECK(stored.rows == 1050);
      CHECK(stored.cols == 10965);
      CHECK(stored.at<uint16_t>(0, 0) == 16275);
    } else {
      CHECK(false);
    }
  } else {
    CHECK(false);
  }

  DicomScan siemens;
  const bool siemens_ok = LoadDicomScan(siemens_path.string(), siemens, error);
  CHECK(siemens_ok);
  if (!siemens_ok) {
    std::cerr << error << "\n";
  } else if (siemens.frames.size() == 1 && !siemens.frames[0].empty()) {
    CHECK(siemens.rows == 484);
    CHECK(siemens.cols == 484);
    CHECK(siemens.modality == "MR");
    CHECK(siemens.bits_allocated == 16);
    CHECK(siemens.bits_stored == 12);
    CHECK(siemens.frames[0].rows == 484);
    CHECK(siemens.frames[0].cols == 484);
    CHECK(Near(siemens.spacing_row_mm, 0.72314049586777));
    CHECK(Near(siemens.spacing_col_mm, 0.72314049586777));
    CHECK(cv::countNonZero(siemens.frames[0]) > 0);
  } else {
    CHECK(false);
  }

  DicomScan missing;
  CHECK(!LoadDicomScan((Sample("missing-not-dicom")).string(), missing, error));
  CHECK(!error.empty());
  CHECK(missing.frames.empty());

  const auto tainted_path = std::filesystem::temp_directory_path() / "tape_scanner_tainted_tail.dcm";
  CHECK(saveToDcm(cv::Mat(4, 4, CV_16UC1, cv::Scalar(9)), tainted_path.string()));
  {
    std::fstream tainted(tainted_path, std::ios::binary | std::ios::app);
    const unsigned char junk[] = {0xfc, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff};
    tainted.write(reinterpret_cast<const char *>(junk), sizeof(junk));
  }
  DicomScan tainted_scan;
  const bool tainted_ok = LoadDicomScan(tainted_path.string(), tainted_scan, error);
  CHECK(tainted_ok);
  if (tainted_ok && tainted_scan.frames.size() == 1 && !tainted_scan.frames[0].empty()) {
    CHECK(tainted_scan.frames[0].rows == 4);
    CHECK(tainted_scan.frames[0].at<uint16_t>(0, 0) == 9);
  } else {
    std::cerr << error << "\n";
    CHECK(false);
  }
  std::filesystem::remove(tainted_path);
}

} // namespace

int main() {
  // Писатель и разбор серии должны держать точку даже в русской локали.
  std::setlocale(LC_ALL, "ru_RU.UTF-8");
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_open_scan";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  TestDecimalsAndCharset();
  TestWriters(dir);
  TestLegacySpacingCharsetAndExtensionless(dir);
  TestTilePlan();
  TestSamples();
  std::filesystem::remove_all(dir);
  if (g_failures != 0) {
    std::cerr << g_failures << " checks failed\n";
    return 1;
  }
  std::cout << "ok\n";
  return 0;
}

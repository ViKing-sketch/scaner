#include <tape_scanner/scanner/ExposureSteps.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>
#include <tape_scanner/scanner/Utils.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <opencv2/core.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
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

bool Near(double a, double b) { return std::abs(a - b) < 1e-9; }

std::string ReadFile(const std::string &path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool ImageComments(const std::string &path, std::string &comments) {
  DcmFileFormat file;
  if (file.loadFile(path.c_str()).bad()) {
    return false;
  }
  OFString value;
  if (file.getDataset()->findAndGetOFString(DCM_ImageComments, value).bad()) {
    comments.clear();
    return false;
  }
  comments = value.c_str();
  return true;
}

void TestPlaceholderMapsFortyAndSixtyNineToTheSameStep() {
  const ExposureStepTable &table = ExposureStepTable::Placeholder();
  CHECK(table.IsPlaceholder());
  CHECK(table.Note().find("PLACEHOLDER") != std::string::npos);
  CHECK(table.Note().find("awaiting Max's measured exposure step table") != std::string::npos);
  CHECK(table.Note().find("Not a calibration") != std::string::npos);

  int observed = 0;
  for (const ExposureStep &step : table.Steps()) {
    if (step.origin == ExposureStepOrigin::ObservedExample) {
      ++observed;
      CHECK(step.requested_min_us == 40.0);
      CHECK(step.requested_max_us == 69.0);
      CHECK(step.effective_us == 70.0);
    } else {
      CHECK(step.origin == ExposureStepOrigin::Placeholder);
    }
  }
  CHECK(observed == 1);

  const ExposureMatch at_40 = table.Match(40.0, false);
  const ExposureMatch at_69 = table.Match(69.0, false);
  CHECK(at_40.step_index >= 0);
  CHECK(at_40.step_index == at_69.step_index);
  CHECK(at_40.effective_us == at_69.effective_us);
  CHECK(at_40.effective_us == 70.0);
  CHECK(at_69.effective_us == 70.0);

  const ExposureMatch below = table.Match(39.0, false);
  CHECK(below.step_index >= 0);
  CHECK(below.step_index != at_40.step_index);
  CHECK(below.effective_us != 70.0);

  const ExposureMatch above_table = table.Match(1600.0, false);
  CHECK(above_table.step_index == -1);
  CHECK(above_table.effective_us == 1600.0);

  CHECK(ActiveExposureStepTable().Match(40.0, false).effective_us ==
        ActiveExposureStepTable().Match(69.0, false).effective_us);
}

void TestCustomTableIsUsed() {
  const ExposureStepTable custom({{10.0, 100.0, 50.0, ExposureStepOrigin::Placeholder}}, "custom table for test",
                                 false);
  CHECK(!custom.IsPlaceholder());
  const ExposureMatch at_40 = custom.Match(40.0, false);
  const ExposureMatch at_69 = custom.Match(69.0, false);
  CHECK(at_40.step_index == at_69.step_index);
  CHECK(at_40.effective_us == 50.0);
  CHECK(at_69.effective_us == 50.0);
  CHECK(custom.Match(5.0, false).step_index == -1);
  CHECK(custom.Match(5.0, false).effective_us == 5.0);
}

void TestHdrWeightsByEffectiveStep() {
  cv::Mat pixel(1, 1, CV_16UC1, cv::Scalar(700));
  const cv::Mat from_40 = hdr({{pixel, 40.0}});
  const cv::Mat from_69 = hdr({{pixel, 69.0}});
  CHECK(from_40.type() == CV_64FC1);
  const double weight_40 = from_40.at<double>(0, 0);
  const double weight_69 = from_69.at<double>(0, 0);
  CHECK(Near(weight_40, weight_69));
  CHECK(Near(weight_40, 700.0 / 70.0));
  CHECK(std::abs(weight_40 - (700.0 / 40.0)) > 1.0);

  const ExposureStepTable custom({{10.0, 100.0, 50.0, ExposureStepOrigin::Placeholder}}, "custom table for test",
                                 false);
  cv::Mat other(1, 1, CV_16UC1, cv::Scalar(500));
  const cv::Mat weighted = hdr({{other, 40.0}, {other, 69.0}}, 65535, &custom);
  CHECK(Near(weighted.at<double>(0, 0), 500.0 / 50.0));
}

void TestSessionMetadataRecordsEffectiveExposure() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_exposure_steps";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  const std::string plain_path = (dir / "plain.dcm").string();
  ScanSession plain(plain_path, ScanGeometry{});
  cv::Mat tile(2, 2, CV_16UC1, cv::Scalar(1000));
  CHECK(plain.PushRegion(tile.clone(), 0, 0));
  CHECK(plain.Finish());
  CHECK(!std::filesystem::exists(plain.ExposureMetadataPath()));
  std::string plain_comments;
  CHECK(!ImageComments(plain.SavedPaths().at(0), plain_comments));

  const std::string chosen = (dir / "roll.dcm").string();
  ScanSession session(chosen, ScanGeometry{});
  session.RecordRequestedExposure(40.0);
  session.RecordRequestedExposure(69.0);

  const SessionExposureMetadata &meta = session.ExposureMetadata();
  CHECK(meta.table_placeholder);
  CHECK(meta.table_note.find("PLACEHOLDER") != std::string::npos);
  CHECK(meta.exposures.size() == 2);
  CHECK(meta.exposures[0].requested_us == 40.0);
  CHECK(meta.exposures[1].requested_us == 69.0);
  CHECK(meta.exposures[0].effective_us == 70.0);
  CHECK(meta.exposures[1].effective_us == 70.0);
  CHECK(meta.exposures[0].step_index == meta.exposures[1].step_index);
  CHECK(meta.exposures[0].step_index >= 0);
  CHECK(session.ExposureMetadataPath() == MakeSessionMetadataPath(chosen));

  const std::string json = ReadFile(session.ExposureMetadataPath());
  CHECK(json.find("\"exposure_step_table\": \"placeholder\"") != std::string::npos);
  CHECK(json.find("PLACEHOLDER: awaiting Max's measured exposure step table. Not a calibration.") != std::string::npos);
  CHECK(json.find("\"requested_us\": 40") != std::string::npos);
  CHECK(json.find("\"requested_us\": 69") != std::string::npos);
  const auto first_effective = json.find("\"effective_us\": 70");
  CHECK(first_effective != std::string::npos);
  CHECK(json.find("\"effective_us\": 70", first_effective + 1) != std::string::npos);

  CHECK(session.PushRegion(tile.clone(), 0, 0));
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 1);
  std::string comments;
  CHECK(ImageComments(session.SavedPaths().at(0), comments));
  CHECK(comments.find("PLACEHOLDER") != std::string::npos);
  CHECK(comments.find("awaiting Max's measured exposure step table") != std::string::npos);
  CHECK(comments.find("requested_us=40,69") != std::string::npos);
  CHECK(comments.find("effective_us=70,70") != std::string::npos);

  std::filesystem::remove_all(dir);
}

} // namespace

int main() {
  // Камера и SDK не нужны: проверяется таблица шагов, вес HDR и метаданные сессии.
  TestPlaceholderMapsFortyAndSixtyNineToTheSameStep();
  TestCustomTableIsUsed();
  TestHdrWeightsByEffectiveStep();
  TestSessionMetadataRecordsEffectiveExposure();

  if (g_failures != 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "test_exposure_steps ok\n";
  return 0;
}

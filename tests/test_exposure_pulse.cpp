#include <tape_scanner/scanner/ExposurePulse.hpp>
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

void TestPulsePlansShutterWithDefaultMargin() {
  const ExposureShot shot = PlanExposure(20.0);
  CHECK(shot.planned);
  CHECK(shot.pulse_us == 20.0);
  CHECK(shot.margin_us == kDefaultShutterMarginUs);
  CHECK(shot.shutter_us == 30.0);
  CHECK(shot.shutter_us >= shot.pulse_us);
  CHECK(!shot.pulse_commanded);

  const ExposureShot exact = PlanExposure(20.0, 0.0);
  CHECK(exact.planned);
  CHECK(exact.shutter_us == exact.pulse_us);

  const ExposureShot wider = PlanExposure(20.0, 15.0);
  CHECK(wider.planned);
  CHECK(wider.shutter_us == 35.0);
  CHECK(wider.shutter_us >= wider.pulse_us);

  ControllerPulseSimulator untouched;
  const ExposureShot rejected = CommandExposure(untouched, 20.0, -1.0);
  CHECK(!rejected.planned);
  CHECK(!rejected.pulse_commanded);
  CHECK(untouched.PulsesUs().empty());
  CHECK(!PlanExposure(0.0).planned);
  CHECK(!PlanExposure(-5.0).planned);
}

void TestSimulatorReceivesThePulse() {
  ControllerPulseSimulator controller;
  const ExposureShot first = CommandExposure(controller, 20.0);
  const ExposureShot second = CommandExposure(controller, 40.0);
  CHECK(first.planned);
  CHECK(first.pulse_commanded);
  CHECK(first.pulse_us == 20.0);
  CHECK(first.shutter_us == 30.0);
  CHECK(second.pulse_us == 40.0);
  CHECK(second.shutter_us == 50.0);
  CHECK(controller.PulsesUs().size() == 2);
  CHECK(controller.PulsesUs()[0] == 20.0);
  CHECK(controller.PulsesUs()[1] == 40.0);
  CHECK(controller.CommandLightPulse(0.0) == false);
  CHECK(controller.PulsesUs().size() == 2);
}

void TestHdrWeightsByPulseNotShutter() {
  const ExposureShot shot = PlanExposure(20.0);
  cv::Mat pixel(1, 1, CV_16UC1, cv::Scalar(600));
  const cv::Mat weighted = hdr({{pixel, shot.pulse_us}});
  CHECK(weighted.type() == CV_64FC1);
  const double value = weighted.at<double>(0, 0);
  CHECK(Near(value, 600.0 / 20.0));
  CHECK(std::abs(value - (600.0 / shot.shutter_us)) > 1.0);
}

void TestSessionRecordsPulseAndShutter() {
  const auto dir = std::filesystem::temp_directory_path() / "tape_scanner_exposure_pulse";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);

  cv::Mat tile(2, 2, CV_16UC1, cv::Scalar(1000));
  const std::string plain_path = (dir / "plain.dcm").string();
  ScanSession plain(plain_path, ScanGeometry{});
  CHECK(plain.PushRegion(tile.clone(), 0, 0));
  CHECK(plain.Finish());
  CHECK(!std::filesystem::exists(plain.ExposureMetadataPath()));
  std::string plain_comments;
  CHECK(!ImageComments(plain.SavedPaths().at(0), plain_comments));

  ControllerPulseSimulator controller;
  const ExposureShot shot = CommandExposure(controller, 20.0);
  CHECK(controller.PulsesUs().size() == 1);
  CHECK(controller.PulsesUs()[0] == shot.pulse_us);

  const std::string chosen = (dir / "roll.dcm").string();
  ScanSession session(chosen, ScanGeometry{});
  session.RecordExposure(shot);
  session.RecordExposure(ExposureShot{});

  const SessionExposureMetadata &meta = session.ExposureMetadata();
  CHECK(meta.model == "controller-light-pulse");
  CHECK(meta.note.find("DIAGNOSTIC") != std::string::npos);
  CHECK(meta.note.find("step table is retired") != std::string::npos);
  CHECK(meta.exposures.size() == 1);
  CHECK(meta.exposures[0].pulse_us == 20.0);
  CHECK(meta.exposures[0].shutter_us == 30.0);
  CHECK(meta.exposures[0].shutter_us >= meta.exposures[0].pulse_us);
  CHECK(meta.exposures[0].pulse_commanded);

  const std::string json = ReadFile(session.ExposureMetadataPath());
  CHECK(json.find("\"exposure_model\": \"controller-light-pulse\"") != std::string::npos);
  CHECK(json.find("TODO until Z04") != std::string::npos);
  CHECK(json.find("step table is retired") != std::string::npos);
  CHECK(json.find("\"pulse_us\": 20") != std::string::npos);
  CHECK(json.find("\"effective_us\": 20") != std::string::npos);
  CHECK(json.find("\"shutter_us\": 30") != std::string::npos);
  CHECK(json.find("\"pulse_commanded\": true") != std::string::npos);

  CHECK(session.PushRegion(tile.clone(), 0, 0));
  CHECK(session.Finish());
  std::string comments;
  CHECK(ImageComments(session.SavedPaths().at(0), comments));
  CHECK(comments.find("effective_us=20") != std::string::npos);
  CHECK(comments.find("shutter_us=30") != std::string::npos);
  CHECK(comments.find("TODO until Z04") != std::string::npos);

  std::filesystem::remove_all(dir);
}

} // namespace

int main() {
  // Камера и SDK не нужны. Импульс идёт через имитатор контроллера.
  TestPulsePlansShutterWithDefaultMargin();
  TestSimulatorReceivesThePulse();
  TestHdrWeightsByPulseNotShutter();
  TestSessionRecordsPulseAndShutter();

  if (g_failures != 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "test_exposure_pulse ok\n";
  return 0;
}

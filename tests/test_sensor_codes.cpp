#include <tape_scanner/scanner/SensorCodes.hpp>

#include <opencv2/core.hpp>

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
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

bool Near(float a, float b) { return std::abs(a - b) < 1e-3f; }

/// Прежняя покадровая кривая: (code - 18) * (65536/3800) с насыщением uint16.
cv::Mat LegacyNonlinearityCurve(const cv::Mat &codes) {
  cv::Mat curved = codes.clone();
  curved -= 18;
  curved *= (65536 / 3800.0);
  return curved;
}

std::string ReadText(const char *path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error(std::string("cannot open ") + path);
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void TestRawCodesStayRaw() {
  cv::Mat raw(2, 3, CV_16UC1);
  const uint16_t samples[] = {0, 18, 100, 2000, 3800, 4095};
  for (int i = 0; i < 6; ++i) {
    raw.at<uint16_t>(i / 3, i % 3) = samples[i];
  }

  const cv::Mat kept = RetainRawSensorCodes(raw);
  const cv::Mat curved = LegacyNonlinearityCurve(raw);
  bool curve_would_change = false;
  for (int i = 0; i < 6; ++i) {
    const uint16_t code = samples[i];
    const uint16_t after = kept.at<uint16_t>(i / 3, i % 3);
    const uint16_t legacy = curved.at<uint16_t>(i / 3, i % 3);
    CHECK(after == code);
    if (legacy != code) {
      curve_would_change = true;
    }
  }
  CHECK(curve_would_change);
  CHECK(kept.at<uint16_t>(0, 1) == 18);
  CHECK(curved.at<uint16_t>(0, 1) == 0);
  CHECK(kept.at<uint16_t>(0, 2) == 100);
  CHECK(curved.at<uint16_t>(0, 2) != 100);
  CHECK(kept.at<uint16_t>(1, 2) == 4095);
  CHECK(curved.at<uint16_t>(1, 2) == 65535);

  cv::Mat roi = raw(cv::Rect(1, 0, 2, 2));
  const cv::Mat kept_roi = RetainRawSensorCodes(roi);
  CHECK(kept_roi.at<uint16_t>(0, 0) == 18);
  CHECK(kept_roi.at<uint16_t>(1, 1) == 4095);
}

void TestLinearMergeDoesNotApplyCurve() {
  cv::Mat short_exp(1, 1, CV_16UC1, cv::Scalar(1000));
  cv::Mat long_exp(1, 1, CV_16UC1, cv::Scalar(2000));
  const std::vector<float> exposures = {100.f, 200.f};

  const cv::Mat radiance = MergeLinearExposures({short_exp, long_exp}, exposures);
  CHECK(Near(radiance.at<float>(0, 0), 10.f));

  const cv::Mat curved_radiance = MergeLinearExposures(
      {LegacyNonlinearityCurve(short_exp), LegacyNonlinearityCurve(long_exp)}, exposures);
  CHECK(std::abs(curved_radiance.at<float>(0, 0) - radiance.at<float>(0, 0)) > 1.f);
}

void TestSaturatedSampleIsDropped() {
  cv::Mat first(1, 2, CV_16UC1);
  cv::Mat second(1, 2, CV_16UC1);
  first.at<uint16_t>(0, 0) = 4095;
  second.at<uint16_t>(0, 0) = 500;
  first.at<uint16_t>(0, 1) = 4095;
  second.at<uint16_t>(0, 1) = 4095;

  const cv::Mat radiance = MergeLinearExposures({first, second}, {10.f, 100.f});
  CHECK(Near(radiance.at<float>(0, 0), 5.f));
  CHECK(Near(radiance.at<float>(0, 1), 409.5f));
}

void TestRejectsBadMergeInput() {
  cv::Mat frame(1, 1, CV_16UC1, cv::Scalar(10));
  bool threw = false;
  try {
    MergeLinearExposures({}, {});
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  threw = false;
  try {
    MergeLinearExposures({frame}, {0.f});
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  cv::Mat other(2, 2, CV_16UC1, cv::Scalar(10));
  threw = false;
  try {
    MergeLinearExposures({frame, other}, {1.f, 2.f});
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  threw = false;
  try {
    RetainRawSensorCodes(cv::Mat(1, 1, CV_8UC1, cv::Scalar(1)));
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  const cv::Mat empty = RetainRawSensorCodes(cv::Mat());
  CHECK(empty.empty());
}

void TestProcessingPathDoesNotApplyCurve() {
#ifndef TAPE_SCANNER_CAMERA_CPP
#error TAPE_SCANNER_CAMERA_CPP is not set
#endif
#ifndef TAPE_SCANNER_SENSOR_CPP
#error TAPE_SCANNER_SENSOR_CPP is not set
#endif
  const char *paths[] = {TAPE_SCANNER_CAMERA_CPP, TAPE_SCANNER_SENSOR_CPP};
  const char *needles[] = {
      "65536/3800",
      "65536 / 3800",
      "65536.0/3800",
      "65536.0 / 3800",
      "createMergeDebevec",
      "createMergeRobertson",
      "-= 18",
      ", m_responses",
  };
  for (const char *path : paths) {
    const std::string text = ReadText(path);
    for (const char *needle : needles) {
      if (text.find(needle) != std::string::npos) {
        std::cerr << "FAIL " << path << " still applies the legacy curve (" << needle << ")\n";
        ++g_failures;
      }
    }
  }

  const std::string camera = ReadText(TAPE_SCANNER_CAMERA_CPP);
  CHECK(camera.find("RetainRawSensorCodes") != std::string::npos);
  CHECK(camera.find("MergeLinearExposures") != std::string::npos);
}

} // namespace

int main() {
  try {
    TestRawCodesStayRaw();
    TestLinearMergeDoesNotApplyCurve();
    TestSaturatedSampleIsDropped();
    TestRejectsBadMergeInput();
    TestProcessingPathDoesNotApplyCurve();
  } catch (const std::exception &ex) {
    std::cerr << "FAIL exception: " << ex.what() << "\n";
    return 1;
  }
  if (g_failures != 0) {
    std::cerr << g_failures << " checks failed\n";
    return 1;
  }
  std::cout << "test_sensor_codes passed\n";
  return 0;
}

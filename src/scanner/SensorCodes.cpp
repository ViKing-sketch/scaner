#include <tape_scanner/scanner/SensorCodes.hpp>

#include <stdexcept>

namespace {

constexpr uint16_t kFullScaleCode = 4095;

void RequireFrame(const cv::Mat &frame, const cv::Size &size) {
  if (frame.empty() || frame.type() != CV_16UC1 || frame.size() != size) {
    throw std::invalid_argument(
        "MergeLinearExposures: expected non-empty CV_16UC1 frames of one size");
  }
}

} // namespace

cv::Mat RetainRawSensorCodes(const cv::Mat &sensor_codes) {
  if (sensor_codes.empty()) {
    return sensor_codes;
  }
  if (sensor_codes.type() != CV_16UC1) {
    throw std::invalid_argument("RetainRawSensorCodes: expected CV_16UC1");
  }
  return sensor_codes;
}

cv::Mat MergeLinearExposures(const std::vector<cv::Mat> &frames,
                             const std::vector<float> &exposure_us) {
  if (frames.empty() || frames.size() != exposure_us.size()) {
    throw std::invalid_argument(
        "MergeLinearExposures: frame and exposure counts differ");
  }
  for (float exposure : exposure_us) {
    if (!(exposure > 0.f)) {
      throw std::invalid_argument("MergeLinearExposures: exposure must be positive");
    }
  }

  const cv::Size size = frames[0].size();
  for (const cv::Mat &frame : frames) {
    RequireFrame(frame, size);
  }

  cv::Mat radiance(size, CV_32FC1);
  for (int y = 0; y < size.height; ++y) {
    float *out = radiance.ptr<float>(y);
    for (int x = 0; x < size.width; ++x) {
      double code_sum = 0.0;
      double exposure_sum = 0.0;
      double fallback_code = 0.0;
      double fallback_exposure = 0.0;
      bool have_fallback = false;

      for (size_t i = 0; i < frames.size(); ++i) {
        const double exposure = exposure_us[i];
        const uint16_t code = frames[i].ptr<uint16_t>(y)[x];
        if (!have_fallback || exposure < fallback_exposure) {
          fallback_code = code;
          fallback_exposure = exposure;
          have_fallback = true;
        }
        if (code >= kFullScaleCode) {
          continue;
        }
        code_sum += code;
        exposure_sum += exposure;
      }

      const double value = exposure_sum > 0.0 ? code_sum / exposure_sum
                                              : fallback_code / fallback_exposure;
      out[x] = static_cast<float>(value);
    }
  }
  return radiance;
}

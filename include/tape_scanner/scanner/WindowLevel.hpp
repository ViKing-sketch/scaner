#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

/// Верх 16-битного беззнакового пикселя. Ползунки и шейдер делят на это число один раз.
inline constexpr double kDisplayMaxValue = 65535.0;

struct WindowLevel {
  double center = kDisplayMaxValue / 2.0;
  double width = kDisplayMaxValue;
};

/// Окно на минимум и максимум кадра: low = min, high = max.
inline WindowLevel WindowFromMinMax(double min_value, double max_value) {
  if (max_value < min_value) {
    std::swap(min_value, max_value);
  }
  WindowLevel window;
  window.width = std::max(1.0, max_value - min_value);
  window.center = (min_value + max_value) / 2.0;
  return window;
}

inline std::pair<int, int> RawMinMax(const cv::Mat &image) {
  if (image.empty() || image.type() != CV_16UC1) {
    return {0, 0};
  }
  double min_value = 0;
  double max_value = 0;
  cv::minMaxLoc(image, &min_value, &max_value);
  return {static_cast<int>(min_value), static_cast<int>(max_value)};
}

/// Серый 0..1 по формуле шейдера, но в единицах пикселя файла.
inline double DisplayGray(double pixel, WindowLevel window, bool invert) {
  const double low = window.center - window.width / 2.0;
  const double high = window.center + window.width / 2.0;
  const double range = std::max(high - low, 1e-6);
  double shown = (pixel - low) / range;
  shown = std::clamp(shown, 0.0, 1.0);
  if (invert) {
    shown = 1.0 - shown;
  }
  return shown;
}

/**
 * То же окно после единственного перевода в 0..1: center/65535, width/65535.
 * Шейдер получает уже эти доли и второй раз на диапазон ползунка не делит.
 */
inline float ShaderDisplay(float pixel, float center, float width, bool invert,
                           float max_value = static_cast<float>(kDisplayMaxValue)) {
  const float sample = pixel / max_value;
  const float norm_center = center / max_value;
  const float norm_width = width / max_value;
  const float low = norm_center - norm_width / 2.f;
  const float high = norm_center + norm_width / 2.f;
  const float range = std::max(high - low, 1e-6f);
  float shown = (sample - low) / range;
  shown = std::clamp(shown, 0.f, 1.f);
  if (invert) {
    shown = 1.f - shown;
  }
  return shown;
}

/// Правая кнопка: вверх увеличивает уровень, вправо — ширину. Полный жест через вид занимает весь 16-битный диапазон.
inline void DragWindow(WindowLevel &window, double dx_px, double dy_px, int view_w, int view_h) {
  window.center += (-dy_px) * (kDisplayMaxValue / static_cast<double>(std::max(view_h, 1)));
  window.width += dx_px * (kDisplayMaxValue / static_cast<double>(std::max(view_w, 1)));
  window.center = std::clamp(window.center, 0.0, kDisplayMaxValue);
  window.width = std::clamp(window.width, 1.0, kDisplayMaxValue);
}

/// Масштаб 1 и крупнее — без сглаживания соседних пикселей.
inline bool UseNearestFilter(float scale) { return scale >= 1.f; }

/// Поворот и отражение только для экрана. Коды отражения: 0 нет, 1 по горизонтали, 2 по вертикали, -1 оба.
inline cv::Mat PresentFrame(const cv::Mat &src, int rotation_deg, int flip) {
  if (src.empty()) {
    return {};
  }
  cv::Mat rotated;
  if (rotation_deg == 90 || rotation_deg == -270) {
    cv::rotate(src, rotated, cv::ROTATE_90_CLOCKWISE);
  } else if (rotation_deg == -90 || rotation_deg == 270) {
    cv::rotate(src, rotated, cv::ROTATE_90_COUNTERCLOCKWISE);
  } else if (rotation_deg == 180 || rotation_deg == -180) {
    cv::rotate(src, rotated, cv::ROTATE_180);
  } else {
    rotated = src;
  }
  if (flip == 0) {
    return rotated;
  }
  cv::Mat flipped;
  const int code = flip == 1 ? 1 : flip == 2 ? 0 : -1;
  cv::flip(rotated, flipped, code);
  return flipped;
}

/// Сырые 16-битные пиксели файла, без VOI и без setMinMaxWindow. Пустая матрица, если это не mono 16.
cv::Mat ReadStoredMono16(const std::string &path, int frame_index = 0);

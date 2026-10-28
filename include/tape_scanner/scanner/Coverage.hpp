#pragma once

#include <opencv2/core.hpp>

#include <cstdint>
#include <string>

/**
 * Маска холста мозаики, не замер ширины поля 70/100 мм.
 *
 * 0 — пиксель холста не заполнен тайлом. Такое число в кадре не является
 * измеренным сигналом, даже если в DICOM там ноль.
 * 255 — отсчёт пришёл из тайла. Ноль при маске 255 — измеренный ноль.
 *
 * Файл рядом с сегментом: `<dicom-path>.coverage`, версия 1, little-endian.
 * Заголовок: magic "TSCV", version u8, reason_len u8, rows u32, cols u32,
 * затем reason_len байт причины и rows*cols байт маски. Причина пустая,
 * только если дыр нет. Иная причина, кроме "unfilled", этим шагом не пишется.
 */

inline constexpr uint8_t kCoverageUnfilled = 0;
inline constexpr uint8_t kCoverageValid = 255;
inline constexpr char kCoverageGapUnfilled[] = "unfilled";
inline constexpr char kCoverageMagic[] = "TSCV";
inline constexpr uint8_t kCoverageFileVersion = 1;

struct IntensityStats {
  bool available = false;
  int valid_count = 0;
  int invalid_count = 0;
  double min_value = 0;
  double max_value = 0;
  double mean = 0;
};

/// Мин/макс/среднее только по ненулевой маске. Дыры в min не попадают.
/// Если измеренных пикселей нет, available == false, а не ноль.
IntensityStats IntensityOnCoverage(const cv::Mat &image, const cv::Mat &coverage);

struct CoveredFrame {
  cv::Mat image;
  cv::Mat coverage;
  IntensityStats stats;
  bool complete = false;
  std::string gap_reason;
};

struct SavedSegmentCoverage {
  std::string dicom_path;
  std::string mask_path;
  int rows = 0;
  int cols = 0;
  bool complete = false;
  std::string gap_reason;
  IntensityStats stats;
};

std::string CoverageMaskPath(const std::string &dicom_path);

bool WriteCoverageMask(const std::string &path, const cv::Mat &coverage, const std::string &gap_reason);

bool ReadCoverageMask(const std::string &path, cv::Mat &coverage, std::string &gap_reason);

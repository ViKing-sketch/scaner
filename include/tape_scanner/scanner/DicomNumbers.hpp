#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

/// DICOM Decimal String / Integer String. Точка, не запятая локали.
inline constexpr const char kDicomUtf8Charset[] = "ISO_IR 192";
inline constexpr const char kDicomPixelAspectRatio[] = "1\\1";

/**
 * Число с фиксированной точкой без хвостовых нулей.
 * std::to_chars не смотрит на локаль: под ru_RU snprintf("%.6f") пишет «0,042».
 */
inline std::string FormatFixedDecimal(double value, int precision) {
  if (!std::isfinite(value)) {
    return "0";
  }
  if (precision < 0) {
    precision = 0;
  }
  if (precision > 12) {
    precision = 12;
  }
  char buf[64];
  const auto result = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::fixed, precision);
  if (result.ec != std::errc()) {
    return "0";
  }
  std::string text(buf, result.ptr);
  const auto dot = text.find('.');
  if (dot != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  if (text.empty() || text == "-" || text == "+") {
    return "0";
  }
  return text;
}

/// DICOM DS: не длиннее 16 символов, десятичная точка.
inline std::string FormatDicomDecimal(double value) {
  std::string text = FormatFixedDecimal(value, 6);
  if (text.size() > 16) {
    text.resize(16);
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  if (text.empty() || text == "-" || text == "+") {
    return "0";
  }
  return text;
}

/**
 * Разбор одного DICOM-числа. Принимает и точку, и запятую.
 * Не зависит от локали (в отличие от strtod).
 */
inline std::optional<double> ParseDicomDecimal(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\0')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\0')) {
    text.remove_suffix(1);
  }
  if (text.empty()) {
    return std::nullopt;
  }
  std::string normalized;
  normalized.reserve(text.size());
  bool saw_separator = false;
  bool saw_digit = false;
  bool saw_sign = false;
  for (unsigned char ch : text) {
    if (ch == '+' || ch == '-') {
      if (saw_sign || saw_digit || saw_separator || !normalized.empty()) {
        return std::nullopt;
      }
      saw_sign = true;
      if (ch == '-') {
        normalized.push_back('-');
      }
      continue;
    }
    if (ch == ',' || ch == '.') {
      if (saw_separator) {
        return std::nullopt;
      }
      saw_separator = true;
      normalized.push_back('.');
      continue;
    }
    if (ch >= '0' && ch <= '9') {
      saw_digit = true;
      normalized.push_back(static_cast<char>(ch));
      continue;
    }
    if (ch == 'e' || ch == 'E') {
      normalized.push_back(static_cast<char>(ch));
      continue;
    }
    return std::nullopt;
  }
  if (!saw_digit) {
    return std::nullopt;
  }
  double value = 0;
  const char *begin = normalized.data();
  const char *end = begin + normalized.size();
  const auto parsed = std::from_chars(begin, end, value, std::chars_format::general);
  if (parsed.ec != std::errc() || parsed.ptr != end || !std::isfinite(value)) {
    return std::nullopt;
  }
  return value;
}

/**
 * Pixel Spacing в миллиметрах.
 * Значение больше 1 мм для плёнки неправдоподобно: старые файлы писали микрометры
 * («42,33» значит 42,33 мкм = 0,04233 мм). Ровно 1 мм остаётся 1 мм.
 */
inline double PixelSpacingToMm(double raw) {
  if (!(raw > 0.0)) {
    return 0;
  }
  if (raw > 1.0) {
    return raw / 1000.0;
  }
  return raw;
}

/**
 * Нарезка длинной полосы под лимит текстуры GPU.
 * Тайл не больше min(max_texture, 4096). Полосы сканера (до ~40968 по длинной стороне)
 * остаются в полном разрешении. Сжатие включается только если тайлов стало бы больше 256.
 */
struct DisplayTilePlan {
  int tile = 1;
  int tiles_x = 1;
  int tiles_y = 1;
  int upload_cols = 0;
  int upload_rows = 0;
  double scale = 1;
};

inline DisplayTilePlan PlanDisplayTiles(int cols, int rows, int max_texture_size) {
  DisplayTilePlan plan;
  if (cols < 1 || rows < 1) {
    return plan;
  }
  int limit = max_texture_size > 0 ? max_texture_size : 4096;
  if (limit > 4096) {
    limit = 4096;
  }
  plan.tile = limit;
  plan.upload_cols = cols;
  plan.upload_rows = rows;
  const auto count_tiles = [&](int width, int height) {
    plan.tiles_x = (width + plan.tile - 1) / plan.tile;
    plan.tiles_y = (height + plan.tile - 1) / plan.tile;
  };
  count_tiles(cols, rows);
  const long long tiles = static_cast<long long>(plan.tiles_x) * static_cast<long long>(plan.tiles_y);
  if (tiles > 256) {
    const double longer = static_cast<double>(cols > rows ? cols : rows);
    const double target = static_cast<double>(limit) * 8.0;
    plan.scale = target / longer;
    if (plan.scale < 1.0) {
      plan.upload_cols = static_cast<int>(std::lround(static_cast<double>(cols) * plan.scale));
      plan.upload_rows = static_cast<int>(std::lround(static_cast<double>(rows) * plan.scale));
      if (plan.upload_cols < 1) {
        plan.upload_cols = 1;
      }
      if (plan.upload_rows < 1) {
        plan.upload_rows = 1;
      }
      count_tiles(plan.upload_cols, plan.upload_rows);
    } else {
      plan.scale = 1;
    }
  }
  return plan;
}

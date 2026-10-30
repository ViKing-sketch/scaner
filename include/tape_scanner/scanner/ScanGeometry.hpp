#pragma once

#include <tape_scanner/scanner/DicomNumbers.hpp>

#include <cmath>
#include <string>

/**
 * Геометрия сохранённой сетки.
 *
 * pixel_um_x / pixel_um_y — оптический шаг сенсора до биннинга, мкм.
 * Номинал до замера по линейке: 21.0 мкм по обеим осям.
 * Замер: мкм = расстояние_мм / число_пикселей * 1000, отдельно по X и по Y.
 * Число пикселей — оптические (до bin). Если отметки сняты с сохранённой сетки,
 * оптических пикселей в bin раз больше. Константу 27/1280 в геометрию не вшивать.
 *
 * bin = 1 — сетка 21 мкм (~1200 dpi), bin = 2 — усреднение 2×2, сетка 42 мкм (~600 dpi).
 * Поле ppm шаг каретки и тег Pixel Spacing не задаёт.
 *
 * Оси DICOM Pixel Spacing (0018,1164), миллиметры: сначала соседние строки, затем
 * соседние столбцы. У этого сканера строки идут вдоль подачи роликов (Y),
 * столбцы — вдоль каретки (X).
 */

inline constexpr double kNominalPixelUm = 21.0;
inline constexpr int kDefaultBin = 2;
/// Верхняя граница Rows/Columns одного файла. DICOM допускает 65535;
/// 60000 оставляет запас и режет рулон до отдельных сегментов в RAM.
inline constexpr int kMaxDicomDimension = 60000;

struct ScanGeometry {
  int bin = kDefaultBin;
  double pixel_um_x = kNominalPixelUm;
  double pixel_um_y = kNominalPixelUm;

  /// Некорректный bin или неположительный шаг заменяются номиналом.
  static ScanGeometry Normalized(ScanGeometry geometry) {
    if (geometry.bin != 1 && geometry.bin != 2) {
      geometry.bin = kDefaultBin;
    }
    if (!(geometry.pixel_um_x > 0.0)) {
      geometry.pixel_um_x = kNominalPixelUm;
    }
    if (!(geometry.pixel_um_y > 0.0)) {
      geometry.pixel_um_y = kNominalPixelUm;
    }
    return geometry;
  }

  /// Шаг соседних строк, мм. При bin = 2 оптический шаг Y удваивается.
  double RowSpacingMm() const { return pixel_um_y * static_cast<double>(bin) / 1000.0; }

  /// Шаг соседних столбцов, мм. При bin = 2 оптический шаг X удваивается.
  double ColSpacingMm() const { return pixel_um_x * static_cast<double>(bin) / 1000.0; }
};

/// Подпись режима: оптический шаг, умноженный на bin. 21,4 мкм и bin = 2 → "42.8".
inline std::string FormatPitchUm(double pixel_um, int bin) {
  return FormatFixedDecimal(pixel_um * static_cast<double>(bin), 4);
}

/// Оптический шаг, мкм. Формула: расстояние_мм / число_пикселей * 1000.
/// pixel_count — пиксели сенсора до биннинга. При ошибке pitch_um не меняется.
inline bool PitchUmFromRuler(double distance_mm, double pixel_count, double &pitch_um) {
  if (!(distance_mm > 0.0) || !(pixel_count > 0.0)) {
    return false;
  }
  pitch_um = distance_mm / pixel_count * 1000.0;
  return true;
}

/// То же, если отметки стоят на сохранённой сетке (после bin).
/// Оптический шаг = (расстояние_мм / пиксели_сетки * 1000) / bin.
inline bool OpticalPitchFromSavedGrid(double distance_mm, double saved_pixels, int bin, double &optical_um) {
  if (bin != 1 && bin != 2) {
    return false;
  }
  return PitchUmFromRuler(distance_mm, saved_pixels * static_cast<double>(bin), optical_um);
}

/// Модельные координаты кадра: левый верхний угол пикселя (0,0) в (−1, +1),
/// дальний угол сетки (cols, rows) в (+1, −1).
inline bool ImagePixelFromModel(double model_x, double model_y, int cols, int rows, double &px, double &py) {
  if (cols <= 0 || rows <= 0) {
    return false;
  }
  px = (model_x + 1.0) * static_cast<double>(cols) / 2.0;
  py = (1.0 - model_y) * static_cast<double>(rows) / 2.0;
  return true;
}

inline void PresentedSize(int src_cols, int src_rows, int rotation_deg, int &cols, int &rows) {
  const bool quarter = rotation_deg == 90 || rotation_deg == -90 || rotation_deg == 270 || rotation_deg == -270;
  cols = quarter ? src_rows : src_cols;
  rows = quarter ? src_cols : src_rows;
}

/// Обратное к PresentFrame: точка показанного кадра → пиксель файла.
/// Поворот, затем отражение. Расстояние между двумя точками от −1 не зависит.
inline bool SourceMarkFromPresented(double presented_x, double presented_y, int src_cols, int src_rows,
                                    int rotation_deg, int flip, double &src_x, double &src_y) {
  if (src_cols <= 0 || src_rows <= 0) {
    return false;
  }
  int presented_cols = 0;
  int presented_rows = 0;
  PresentedSize(src_cols, src_rows, rotation_deg, presented_cols, presented_rows);
  double x = presented_x;
  double y = presented_y;
  if (flip == 1 || flip == -1) {
    x = (presented_cols - 1) - x;
  }
  if (flip == 2 || flip == -1) {
    y = (presented_rows - 1) - y;
  }
  if (rotation_deg == 90 || rotation_deg == -270) {
    src_x = y;
    src_y = (src_rows - 1) - x;
  } else if (rotation_deg == -90 || rotation_deg == 270) {
    src_x = (src_cols - 1) - y;
    src_y = x;
  } else if (rotation_deg == 180 || rotation_deg == -180) {
    src_x = (src_cols - 1) - x;
    src_y = (src_rows - 1) - y;
  } else {
    src_x = x;
    src_y = y;
  }
  return true;
}

/// |x2−x1| по столбцам (каретка, X) или |y2−y1| по строкам (ролик, Y).
inline double AxisPixelSpan(double x1, double y1, double x2, double y2, bool along_x) {
  return std::abs(along_x ? (x2 - x1) : (y2 - y1));
}

#pragma once

#include <string>
#include <vector>

/**
 * Один файл серии, которую сканер нарезал по длине рулона.
 * position_row_mm — средняя координата Image Position Patient (0020,0032).
 */
struct DicomSeriesPart {
  std::string path;
  int instance = 0;
  double position_row_mm = 0;
  unsigned rows = 0;
  unsigned cols = 0;
  double spacing_row_mm = 0;
};

/**
 * Все файлы той же Series UID, что и открытый кадр, по возрастанию смещения строки.
 * Просмотрщик листает их как продолжение одной плёнки: следующий кадр начинается
 * там, где кончился предыдущий.
 * В память попадают только короткие теги. Пиксели соседних файлов остаются на диске.
 */
std::vector<DicomSeriesPart> ListDicomSeries(const std::string &any_segment_path);

/// Предел длины одного элемента при разборе серии.
/// Теги заголовка укладываются. Pixel Data сегмента скана длиннее и не читается.
inline constexpr unsigned kDicomSeriesValueLimit = 4096;

/**
 * Паспорт масштаба открытого снимка. Пишется saveToDcm, когда bin равен 1 или 2.
 * present == false у файла без создателя SCANER: шаг пикселя bin не заменяет.
 */
struct ImageScalePassport {
  bool present = false;
  int bin = 0;
  std::string plane;
  std::string roi;
  int rotation_deg = 0;
  int flip = 0;
};

ImageScalePassport ReadImageScalePassport(const std::string &path);

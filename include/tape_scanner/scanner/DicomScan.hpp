#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <string_view>
#include <vector>

/**
 * Один текстовый тег, уже приведённый к UTF-8.
 * key — как у DCMTK, «(0010,0010)».
 */
struct DicomTagText {
  std::string key;
  std::string name;
  std::string value;
};

/**
 * Загруженный скан, уже отделённый от файла.
 * Просмотрщик кладёт frames в существующий GLImgViewer. Фаза 2 (сырые кадры и
 * имитация камеры/контроллера) может собрать такой же набор cv::Mat и отдать
 * его в тот же просмотрщик, не трогая путь живого сканера.
 */
struct DicomScan {
  std::vector<cv::Mat> frames;
  int rows = 0;
  int cols = 0;
  int bits_allocated = 0;
  int bits_stored = 0;
  int high_bit = 0;
  int samples_per_pixel = 1;
  std::string photometric;
  std::string modality;
  std::string sop_class_uid;
  std::string patient_name;
  std::string charset;
  double spacing_row_mm = 0;
  double spacing_col_mm = 0;
  bool monochrome1 = false;
  std::vector<DicomTagText> tags;
};

/// Преамбула DICM по смещению 128. Так узнаём файл без расширения (Iberis/Microtek).
bool FileHasDicomMarker(const std::string &path);

/**
 * Байты тега -> UTF-8.
 * ISO_IR 192, ISO_IR 144, ISO_IR 100 и пустой набор (тогда байты >= 0x80 — cp1251,
 * как у старых Secondary Capture и приватной группы 7001).
 */
std::string DecodeDicomString(std::string_view bytes, std::string_view specific_character_set);

/**
 * Читает DICOM в кадры без VOI.
 * Терпит ARRAY: изображение целиком в (2020,0110), разделители с длиной 0xFFFFFFFF,
 * битый (FFFC,FFFC) в хвосте. Стандартный файл (Siemens и наш Secondary Capture) тоже.
 * @return false и текст в error, если это не изображение.
 */
bool LoadDicomScan(const std::string &path, DicomScan &scan, std::string &error);

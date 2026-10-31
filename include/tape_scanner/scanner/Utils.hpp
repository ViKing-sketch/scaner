#pragma once

#include <opencv2/core/types.hpp>
#include <string>

cv::Point2d calcShiftBetweenCadres(const cv::Mat &left, const cv::Mat &right);

int safeStrToInt(const std::string &str_num);

double safeStrToDouble(const std::string &str_double);

cv::Mat hdr(std::vector<std::pair<cv::Mat, double> > cadres, int pix_max_val = 65535);

cv::Mat readMatFromFile(std::string key, std::string file_name = "test.xml");

void writeMatTo(const cv::Mat& img, const std::string& key = "test", const std::string& file_name = "test");

/**
 * Биннинг кадра до сшивки.
 * bin = 1 возвращает тот же заголовок Mat (без копии пикселей).
 * bin = 2 — среднее окна 2×2 в uint32_t с округлением до ближайшего
 * (половина вверх), результат CV_16UC1. Неполный край при нечётном размере отбрасывается.
 * Прореживание «каждый второй пиксель» не используется.
 */
cv::Mat ApplyScanBinning(const cv::Mat &src, int bin);

/**
 * Метаданные одного кадра Secondary Capture.
 * Пустые UID генерируются при записи. Study/Series одной серии скана
 * нужно передавать одинаковыми, SOP — своим на каждый файл (или пустым).
 *
 * pixel_spacing_* — миллиметры. По умолчанию 0.042 мм: номинал 21 мкм и bin = 2.
 */
struct DicomWriteParams {
  std::string study_instance_uid;
  std::string series_instance_uid;
  std::string sop_instance_uid;
  int instance_number = 1;
  double pixel_spacing_row_mm = 0.042;
  double pixel_spacing_col_mm = 0.042;
  /// Image Position Patient (0020,0032), средняя координата: индекс первой строки сегмента × шаг строки, мм.
  double image_position_row_mm = 0;
  /// UTF-8. Пустое имя записывается как Anonymous. В файл всегда кладётся ISO_IR 192.
  std::string patient_name_utf8;
  std::string patient_id_utf8;
};

/**
 * @brief Сохраняет CV_16UC1 в DICOM Secondary Capture без перевода в 8 бит.
 * @param image     Одноканальное 16-битное изображение. Rows и Columns не больше 60000.
 * @param filename  Путь DICOM-файла.
 * @param params    UID серии, номер экземпляра и Pixel Spacing.
 * @return true     Файл записан.
 * @return false    Пустой кадр, чужой тип, слишком большой размер или ошибка DCMTK.
 */
bool saveToDcm(const cv::Mat &image, const std::string &filename, const DicomWriteParams &params = {});


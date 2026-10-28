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
 * pixel_spacing_* — миллиметры, параметр записи. Поле по умолчанию 0.042 мм оставлено для
 * уже существующих вызовов saveToDcm без геометрии (номинал 21 мкм и bin = 2).
 * Импорт BIN этот пропуск не использует: ConvertBinToDcm требует шаг аргументом.
 *
 * bin 1 или 2 записывает паспорт масштаба (приватный создатель SCANER). 0 — тегов нет.
 * Pixel Spacing сам по себе bin не хранит и не восстанавливает.
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
  /// 0 — не писать паспорт. 1 или 2 — bin сохранённой сетки.
  int bin = 0;
  /// Плоскость замера. Пустая строка при записи паспорта становится "film".
  std::string plane;
  /// ROI на сохранённой сетке, "x\\y\\cols\\rows". Пустая строка — весь кадр.
  std::string roi;
  /// Преобразование получения, не поворот окна просмотра.
  int rotation_deg = 0;
  int flip = 0;
};

/**
 * @brief Сохраняет CV_16UC1 в DICOM Secondary Capture без перевода в 8 бит.
 * @param image     Одноканальное 16-битное изображение. Rows и Columns не больше 60000.
 * @param filename  Путь DICOM-файла.
 * @param params    UID серии, номер экземпляра, Pixel Spacing (0028,0030)
 *                  и Imager Pixel Spacing (0018,1164). Оба тега получают один шаг.
 * @return true     Файл записан.
 * @return false    Пустой кадр, чужой тип, слишком большой размер или ошибка DCMTK.
 */
bool saveToDcm(const cv::Mat &image, const std::string &filename, const DicomWriteParams &params = {});


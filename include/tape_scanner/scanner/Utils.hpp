#pragma once

#include <opencv2/core/types.hpp>

cv::Point2d calcShiftBetweenCadres(const cv::Mat &left, const cv::Mat &right);

int safeStrToInt(const std::string &str_num);

double safeStrToDouble(const std::string &str_double);

cv::Mat hdr(std::vector<std::pair<cv::Mat, double> > cadres, int pix_max_val = 65535);

cv::Mat readMatFromFile(std::string key, std::string file_name = "test.xml");

void writeMatTo(const cv::Mat& img, const std::string& key = "test", const std::string& file_name = "test");

/**
 * @brief Сохраняет изображение cv::Mat в DICOM-файл.
 *  * @param image     Входное изображение OpenCV.
 * @param filename  Путь для сохранения DICOM-файла.
 * @return true     Успешное сохранение.
 * @return false    Ошибка при сохранении.
 */
bool saveToDcm(const cv::Mat &image, const std::string &filename);


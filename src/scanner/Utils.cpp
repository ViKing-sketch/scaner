#include <tape_scanner/scanner/Utils.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <wx/log.h>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>

cv::Point2d calcShiftBetweenCadres(const cv::Mat &left, const cv::Mat &right) {
  if (left.size() != right.size()) {
    throw std::logic_error("Left and right cadres must be same sizes");
  }

  cv::Mat left_f, right_f;

  left.convertTo(left_f, CV_32FC1);
  right.convertTo(right_f, CV_32FC1);

  auto shift = cv::phaseCorrelate(left_f, right_f);

  return shift;
}

int safeStrToInt(const std::string &str_num) {
  int res = 0;
  try {
    std::stringstream stream(str_num);
    stream >> res;
  } catch (std::runtime_error &e) {
    wxLogError(wxString::Format("Convert error: %s", str_num));
  }

  return res;
}


double safeStrToDouble(const std::string &str_double)
{
  double res = 0;
  try {
    std::stringstream stream(str_double);
    stream >> res;
  } catch (std::runtime_error &e) {
    wxLogError(wxString::Format("Convert error: %s", str_double));
  }

  return res;
}

cv::Mat hdr(std::vector<std::pair<cv::Mat, double>> cadres, int pix_max_val) {
  assert(cadres.size());
  assert(cadres[0].first.type() == CV_16UC1);

  cv::Size res_size = cadres[0].first.size();
  cv::Mat res = cv::Mat::zeros(res_size, CV_64FC1);
  cv::Mat cnts = cv::Mat::zeros(res_size, CV_8UC1);

  // int percent = pix_max_val * 0.001;
  int min_treshold = 100;
  int max_treshold = pix_max_val;

  for( const auto &[cadr, expose]: cadres ){
    for(int y = 0; y < cadr.rows; ++y){
      for(int x = 0; x < cadr.cols; ++x){
        double pix_val = cadr.at<uint16_t>(y, x);
        double& res_pix = res.at<double>(y, x);

        if(pix_val < min_treshold)
          continue;

        if(pix_val > max_treshold && cnts.at<uint8_t>(y, x) != 0)
          continue;

        res_pix = (pix_val / expose);
        cnts.at<uint8_t>(y, x)++;
      }
    }
  }

  // for(int y = 0; y < cnts.rows; ++y){
  //   for(int x = 0; x < cnts.cols; ++x){
  //     auto pix_val = cnts.at<uint8_t>(y, x);

  //     if(!pix_val){
  //       // wxLogError(wxString::Format("hdr: pix (%i, %i) is zero!", x, y));
  //       cnts.at<uint8_t>(y, x) = 1; // Чтобы не было ошибки деления на ноль
  //     }
  //   }
  // }

  cnts.convertTo(cnts, CV_64FC1);

  bool save = false;
  if (save) {
    cv::FileStorage fs("test.xml", cv::FileStorage::APPEND);
    fs << "cnts" << cnts;
    fs.release();
  }

  // res = res.mul(1 / cnts);

  return res;
}

cv::Mat readMatFromFile(std::string key, std::string file_name) {
  cv::Mat img;
  cv::FileStorage fs(file_name, cv::FileStorage::READ);
  if(!fs.isOpened())
  {
    wxLogError("Open %s for read failed", file_name);
    return img;
  }
  if(!fs[key].empty()){
    fs[key] >> img;
    wxLogMessage("Read img %s done", key);
  }
  else {
    wxLogMessage("Key %s do not exist in %", key, file_name);
  }
  fs.release();

  return img;
};

void writeMatTo(const cv::Mat& img, const std::string& key, const std::string& file_name) {
  wxLogMessage("Start write img %s to %s", key, file_name);

  cv::FileStorage fs(file_name, cv::FileStorage::WRITE);
  fs << key << img;
  fs.release();

  wxLogMessage("Write img %s to %s done", key, file_name);
}

bool saveToDcm(const cv::Mat& image, const std::string& filename) {
  // 1. Проверка валидности изображения
  if (image.empty()) {
    std::cerr << "Ошибка: пустое изображение" << std::endl;
    return false;
  }

         // 2. Создание формата DICOM-файла и получение dataset
  DcmFileFormat fileFormat;
  DcmDataset *dataset = fileFormat.getDataset();

         // 3. Заполнение обязательных DICOM-тегов
         //    Примерные метаданные — в реальном проекте их нужно заполнять корректно
  dataset->putAndInsertString(DCM_PatientName, "Anonymous");
  dataset->putAndInsertString(DCM_PatientID, "123456");


         // 4. Задание размеров изображения
  int rows = image.rows;
  int cols = image.cols;
  dataset->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(rows));
  dataset->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(cols));

         // 5. Настройки пиксельных данных в зависимости от типа изображения
  int channels = image.channels();
  int depth = image.depth();

         // Поддержка 16-битных изображений
  if (depth != CV_16U || channels != 1) {
    std::cerr << "Ошибка: поддерживаются только 16-битные одноканальные изображения" << std::endl;
    return false;
  }

         // Установка PhotometricInterpretation
  dataset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);

         // Базовые настройки битов
  dataset->putAndInsertUint16(DCM_BitsAllocated, 16);
  dataset->putAndInsertUint16(DCM_BitsStored, 16);
  dataset->putAndInsertUint16(DCM_HighBit, 15);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0); // 0 = unsigned

         // 6. Подготовка данных пикселей
  size_t pixelCount = rows * cols * channels;
  Uint16 *pixelData = new Uint16[pixelCount];

         // Для одноканальных изображений просто копируем
  memcpy(pixelData, image.data, pixelCount * sizeof(pixelData[0]));

         // 7. Вставка пиксельных данных в dataset
  OFCondition status = dataset->putAndInsertUint16Array(DCM_PixelData, pixelData, pixelCount);
  delete[] pixelData;

  if (status.bad()) {
    std::cerr << "Ошибка вставки пиксельных данных: " << status.text() << std::endl;
    return false;
  }

         // 8. Сохранение файла (формат Little Endian Explicit)
  status = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit);
  if (status.bad()) {
    std::cerr << "Ошибка сохранения DICOM-файла: " << status.text() << std::endl;
    return false;
  }

  return true;
}

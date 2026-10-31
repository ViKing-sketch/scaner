#include <tape_scanner/scanner/Utils.hpp>
#include <tape_scanner/scanner/DicomFileMeta.hpp>
#include <tape_scanner/scanner/DicomNumbers.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tape_scanner/scanner/ScanLog.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>

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
    ScanLogError("Convert error: " + str_num);
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
    ScanLogError("Convert error: " + str_double);
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
    ScanLogError("Open " + file_name + " for read failed");
    return img;
  }
  if(!fs[key].empty()){
    fs[key] >> img;
    ScanLogMessage("Read img " + key + " done");
  }
  else {
    ScanLogMessage("Key " + key + " do not exist in " + file_name);
  }
  fs.release();

  return img;
};

void writeMatTo(const cv::Mat& img, const std::string& key, const std::string& file_name) {
  ScanLogMessage("Start write img " + key + " to " + file_name);

  cv::FileStorage fs(file_name, cv::FileStorage::WRITE);
  fs << key << img;
  fs.release();

  ScanLogMessage("Write img " + key + " to " + file_name + " done");
}

namespace {

std::string MakeUid(const char *root) {
  char buf[100];
  return std::string(dcmGenerateUniqueIdentifier(buf, root));
}

/**
 * Среднее 2×2.
 * sum считается в uint32_t: 4 * 65535 = 262140, это не влезает в uint16
 * и не должно обрезаться до 8 бит. Деление (sum + 2) / 4 — округление
 * до ближайшего, половина вверх.
 */
cv::Mat BinAverage2x2(const cv::Mat &src) {
  const int out_rows = src.rows / 2;
  const int out_cols = src.cols / 2;
  cv::Mat dst(out_rows, out_cols, CV_16UC1);
  for (int y = 0; y < out_rows; ++y) {
    const uint16_t *row0 = src.ptr<uint16_t>(2 * y);
    const uint16_t *row1 = src.ptr<uint16_t>(2 * y + 1);
    uint16_t *out = dst.ptr<uint16_t>(y);
    for (int x = 0; x < out_cols; ++x) {
      const uint32_t sum = static_cast<uint32_t>(row0[2 * x]) + row0[2 * x + 1] +
                           row1[2 * x] + row1[2 * x + 1];
      out[x] = static_cast<uint16_t>((sum + 2u) / 4u);
    }
  }
  return dst;
}

} // namespace

cv::Mat ApplyScanBinning(const cv::Mat &src, int bin) {
  if (src.empty()) {
    return {};
  }
  if (src.type() != CV_16UC1) {
    throw std::invalid_argument("ApplyScanBinning: expected CV_16UC1");
  }
  if (bin == 1) {
    return src;
  }
  if (bin != 2) {
    throw std::invalid_argument("ApplyScanBinning: bin must be 1 or 2");
  }
  return BinAverage2x2(src);
}

bool saveToDcm(const cv::Mat &image, const std::string &filename, const DicomWriteParams &params) {
  if (image.empty()) {
    std::cerr << "Ошибка: пустое изображение" << std::endl;
    return false;
  }
  if (filename.empty()) {
    std::cerr << "Ошибка: пустой путь DICOM" << std::endl;
    return false;
  }

  const int rows = image.rows;
  const int cols = image.cols;
  if (image.type() != CV_16UC1) {
    std::cerr << "Ошибка: поддерживаются только 16-битные одноканальные изображения" << std::endl;
    return false;
  }
  if (rows > kMaxDicomDimension || cols > kMaxDicomDimension || rows > 65535 || cols > 65535) {
    std::cerr << "Ошибка: Rows/Columns больше предела одного DICOM-кадра" << std::endl;
    return false;
  }
  if (!(params.pixel_spacing_row_mm > 0.0) || !(params.pixel_spacing_col_mm > 0.0)) {
    std::cerr << "Ошибка: Pixel Spacing должен быть положительным" << std::endl;
    return false;
  }
  if (params.instance_number < 1) {
    std::cerr << "Ошибка: Instance Number начинается с 1" << std::endl;
    return false;
  }

  cv::Mat continuous_holder;
  const cv::Mat *pixels = &image;
  if (!image.isContinuous()) {
    continuous_holder = image.clone();
    pixels = &continuous_holder;
  }

  const std::string study_uid =
      params.study_instance_uid.empty() ? MakeUid(SITE_STUDY_UID_ROOT) : params.study_instance_uid;
  const std::string series_uid =
      params.series_instance_uid.empty() ? MakeUid(SITE_SERIES_UID_ROOT) : params.series_instance_uid;
  const std::string sop_uid =
      params.sop_instance_uid.empty() ? MakeUid(SITE_INSTANCE_UID_ROOT) : params.sop_instance_uid;
  const std::string spacing =
      FormatDicomDecimal(params.pixel_spacing_row_mm) + "\\" + FormatDicomDecimal(params.pixel_spacing_col_mm);
  const std::string image_position = std::string("0\\") + FormatDicomDecimal(params.image_position_row_mm) + "\\0";
  const std::string instance_number = std::to_string(params.instance_number);
  const std::string patient = params.patient_name_utf8.empty() ? std::string("Anonymous") : params.patient_name_utf8;
  const std::string patient_id = params.patient_id_utf8.empty() ? std::string("123456") : params.patient_id_utf8;

  DcmFileFormat fileFormat;
  DcmDataset *dataset = fileFormat.getDataset();
  DcmMetaInfo *meta = fileFormat.getMetaInfo();

  meta->putAndInsertString(DCM_MediaStorageSOPClassUID, UID_SecondaryCaptureImageStorage);
  meta->putAndInsertString(DCM_MediaStorageSOPInstanceUID, sop_uid.c_str());
  meta->putAndInsertString(DCM_TransferSyntaxUID, UID_LittleEndianExplicitTransferSyntax);

  dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
  dataset->putAndInsertString(DCM_SOPInstanceUID, sop_uid.c_str());
  dataset->putAndInsertString(DCM_StudyInstanceUID, study_uid.c_str());
  dataset->putAndInsertString(DCM_SeriesInstanceUID, series_uid.c_str());
  dataset->putAndInsertString(DCM_InstanceNumber, instance_number.c_str());
  dataset->putAndInsertString(DCM_Modality, "OT");
  dataset->putAndInsertString(DCM_ConversionType, "DF");
  dataset->putAndInsertString(DCM_PatientName, patient.c_str());
  dataset->putAndInsertString(DCM_PatientID, patient_id.c_str());
  dataset->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME2");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
  dataset->putAndInsertUint16(DCM_BitsAllocated, 16);
  dataset->putAndInsertUint16(DCM_BitsStored, 16);
  dataset->putAndInsertUint16(DCM_HighBit, 15);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
  dataset->putAndInsertString(DCM_PixelSpacing, spacing.c_str());
  dataset->putAndInsertString(DCM_ImagePositionPatient, image_position.c_str());
  dataset->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(rows));
  dataset->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(cols));

  const size_t pixel_count = static_cast<size_t>(rows) * static_cast<size_t>(cols);
  OFCondition status = dataset->putAndInsertUint16Array(
      DCM_PixelData, reinterpret_cast<const Uint16 *>(pixels->data), static_cast<unsigned long>(pixel_count));
  if (status.bad()) {
    std::cerr << "Ошибка вставки пиксельных данных: " << status.text() << std::endl;
    return false;
  }

  MatchDicomMediaStorage(meta, dataset);
  status = fileFormat.saveFile(filename.c_str(), EXS_LittleEndianExplicit, EET_UndefinedLength, EGL_recalcGL,
                                EPD_noChange, 0, 0, EWM_updateMeta);
  if (status.bad()) {
    std::cerr << "Ошибка сохранения DICOM-файла: " << status.text() << std::endl;
    return false;
  }

  return true;
}

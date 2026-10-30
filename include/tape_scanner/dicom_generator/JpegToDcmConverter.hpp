#pragma once

#include <fstream>
#include <optional>
#include <string>

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h> /* for DCM_ defines */
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/libi2d/i2d.h>
#include <dcmtk/dcmdata/libi2d/i2djpgs.h>
#include <dcmtk/dcmdata/libi2d/i2dplsc.h>
#include <tape_scanner/scanner/DicomFileMeta.hpp>

namespace tape_scanner::dicom_generator {

using std::ifstream;
using std::nullopt;
using std::optional;
using std::runtime_error;
using std::string;
using string_opt = optional<string>;

/**
 * @brief ConvertJpegToDcm - сделать из jpeg изображения dicom файл.
 * @param in_jpeg_file_path - путь до входного jpeg изображения.
 * @param out_dcm_file_path - опциональный путь до выходного dicom файла. Если не указан автоматически формируется из
 * входного пути добавленем в конец расширения *.dcm
 * @return путь до выходного dicom файла
 */
string ConvertJpegToDcm(string in_jpeg_file_path, string_opt out_dcm_file_path = nullopt) {
  if (ifstream(in_jpeg_file_path).bad())
    throw runtime_error("ERROR: Input JPEG file doesn't exists.\n");

  out_dcm_file_path = out_dcm_file_path ? out_dcm_file_path : in_jpeg_file_path + ".dcm";

  auto inputPlug = std::make_unique<I2DJpegSource>();
  auto outPlug = std::make_unique<I2DOutputPlugSC>();
  char uid[100];
  Image2Dcm i2d;
  E_TransferSyntax writeXfer;
  DcmDataset *dataset = NULL;
  OFCondition condition;

  inputPlug->setImageFile(in_jpeg_file_path.c_str());
  condition = i2d.convertFirstFrame(inputPlug.get(), outPlug.get(), 1, dataset, writeXfer);
  if (condition.bad()) {
    throw runtime_error("ERROR: Failed to convert JPEG to DICOM file.\n");
  }
  dataset->putAndInsertString(DCM_PatientName, "JPEG^TO^DICOM");
  dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
  dataset->putAndInsertString(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT));
  DcmFileFormat dcmff(dataset);
  MatchDicomMediaStorage(dcmff.getMetaInfo(), dataset);
  condition = dcmff.saveFile(out_dcm_file_path->c_str(), writeXfer, EET_ExplicitLength, EGL_recalcGL, EPD_noChange,
                             OFstatic_cast(Uint32, 0), OFstatic_cast(Uint32, 0), EWM_updateMeta);
  if (condition.bad()) {
    throw runtime_error("ERROR: Failed to save resulting DICOM file.");
  }

  return *out_dcm_file_path;
}

} // namespace tape_scanner::dicom_generator

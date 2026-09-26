#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "DicomGeneratorUtils.hpp"
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcxfer.h>

namespace tape_scanner::dicom_generator {

using std::runtime_error;
using std::string;
using std::vector;

/**
 * @brief CreateDcm - создать тестовый DCM файл по заданным параметрам.
 * @param dest_file_path - путь до результирующего файла.
 * @param width - ширина фрейма.
 * @param height - высота фрейма.
 * @param bits - количесво бит для хранения одного пикселя (как правило, 8 или 16).
 * @param frames - количество фреймов.
 * @param fill_value - каким значением заполнить каждый пиксель.
 * @param fill_value_max - максимальное значение пикселя. Используется при инкрементировании.
 * @param fill_value_increment - на какое значение инкрементировать значение следующего пикселя.
 * @param increment_for_new_frames_only - инкрементировать все значения пикселей только для каждого нового фрейма.
 */
void CreateDcm(string dest_file_path, size_t width, size_t height, size_t bits, size_t frames, size_t fill_value,
               size_t fill_value_max, size_t fill_value_increment, bool increment_for_new_frames_only) {

  if (!width) {
    width = 1;
  } else if (width > FRAME_MAX_WIDTH) {
    width = FRAME_MAX_WIDTH;
  }

  if (!height) {
    height = 1;
  } else if (height > FRAME_MAX_HEIGHT) {
    height = FRAME_MAX_HEIGHT;
  }

  if (!frames) {
    frames = 1;
  } else if (frames > FRAME_MAX_COUNT) {
    frames = FRAME_MAX_COUNT;
  }

  // calculate minimum input file size
  size_t frame_bin_size = width * height * (bits / 8);
  size_t frames_bin_size = frame_bin_size * frames;
  // vars allocation
  char cstr[256];
  E_TransferSyntax writeXfer = EXS_LittleEndianImplicit;
  DcmFileFormat dcm_file;
  DcmDataset *dataset = dcm_file.getDataset();
  DcmMetaInfo *metainfo = dcm_file.getMetaInfo();
  OFCondition condition;
  size_t frame;

  vector<Uint8> buf(frames_bin_size);

  // setup general info
  // see info here - https://dicom.innolitics.com/ciods/
  metainfo->putAndInsertOFStringArray(DCM_TransferSyntaxUID, UID_LittleEndianImplicitTransferSyntax);
  // setup general tags
  dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
  dataset->putAndInsertOFStringArray(DCM_SeriesDescription, "Auto-generated (parametric)");
  dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, dcmGenerateUniqueIdentifier(cstr, SITE_SERIES_UID_ROOT));
  dataset->putAndInsertOFStringArray(DCM_Modality, "XC");       // External-camera Photography
  dataset->putAndInsertOFStringArray(DCM_ConversionType, "SI"); // Scanned Image
  dataset->putAndInsertOFStringArray(DCM_PatientName, "PARAMETRIC^DICOM^GENERATOR");
  dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(cstr, SITE_INSTANCE_UID_ROOT));
  dataset->putAndInsertOFStringArray(DCM_StudyID, dcmGenerateUniqueIdentifier(cstr, SITE_STUDY_UID_ROOT));
  dataset->putAndInsertOFStringArray(DCM_BurnedInAnnotation, "NO");
  dataset->putAndInsertOFStringArray(DCM_NominalScannedPixelSpacing, "1.0\\1.0"); // mm, row\column
  dataset->putAndInsertOFStringArray(DCM_PixelSpacing, "1.0\\1.0");               // mm, row\column
  dataset->putAndInsertOFStringArray(DCM_PixelAspectRatio, "[1, 1]");             // [row, column]
  dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
  dataset->putAndInsertOFStringArray(DCM_RescaleIntercept, "0");
  dataset->putAndInsertOFStringArray(DCM_RescaleSlope, "1");
  dataset->putAndInsertOFStringArray(DCM_RescaleType, "US");
  dataset->putAndInsertOFStringArray(DCM_PresentationLUTShape, "IDENTITY");
  dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
  dataset->putAndInsertUint16(DCM_BitsAllocated, bits > 8 ? 16 : 8);
  dataset->putAndInsertUint16(DCM_BitsStored, bits);
  dataset->putAndInsertUint16(DCM_HighBit, bits - 1);
  dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
  dataset->putAndInsertUint16(DCM_Rows, height);
  dataset->putAndInsertUint16(DCM_Columns, width);
  // setup multi-frame DICOM file
  if (frames > 1) {
    // setup storage
    dataset->putAndInsertOFStringArray(DCM_SOPClassUID, (bits > 8)
                                                            ? UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage
                                                            : UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage);
    // setup frames
    OFStandard::snprintf(cstr, 30, "%zu", frames);
    dataset->putAndInsertOFStringArray(DCM_NumberOfFrames, cstr);
    dataset->putAndInsertOFStringArray(DCM_FrameTime, "1000");
    dataset->putAndInsertTagKey(DCM_FrameIncrementPointer, DCM_FrameTime);
    // setup single-frame DICOM file
  } else {
    // setup storage
    dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
  }
  if (fill_value == UINT32_MAX) {
    // fill buffer with random values
    std::generate(buf.begin(), buf.end(), []() { return rand() % 256; });
  } else {
    // fill buffer with exact values
    size_t value = fill_value;
    for (frame = 0; frame < frames; frame++) {
      size_t bytes_per_value = bits / 8;
      for (size_t byte = 0; byte < frame_bin_size; byte += bytes_per_value) {
        if (bytes_per_value > 1)
          *((Uint16 *)&buf[frame_bin_size * frame + byte]) = value;
        else
          buf[frame_bin_size * frame + byte] = value;
        if (fill_value_increment && !increment_for_new_frames_only) {
          value += fill_value_increment;
          if (value > fill_value_max)
            value = 0;
        }
      }
      if (fill_value_increment && increment_for_new_frames_only) {
        value += fill_value_increment;
        if (value > fill_value_max)
          value = 0;
      }
    }
  }
  // insert frames binary data
  dataset->putAndInsertUint8Array(DCM_PixelData, buf.data(), frames_bin_size);
  // save destination file
  condition = dcm_file.saveFile((const OFFilename)dest_file_path.c_str(), writeXfer, EET_ExplicitLength, EGL_recalcGL,
                                EPD_noChange, OFstatic_cast(Uint32, 0), OFstatic_cast(Uint32, 0), EWM_fileformat);
  if (condition.bad()) {
    throw runtime_error("ERROR: Failed to save resulting DICOM file.\n");
  }
}

} // namespace tape_scanner::dicom_generator

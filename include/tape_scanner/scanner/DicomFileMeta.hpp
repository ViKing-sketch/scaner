#pragma once

#include <tape_scanner/scanner/DicomNumbers.hpp>

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcmetinf.h>

/**
 * Media Storage SOP Class/Instance в мета-заголовке должны совпадать с SOP Class/Instance
 * датасета. EWM_fileformat не переписывает уже заполненный UID, поэтому перед записью
 * копируем значения из датасета и сохраняем файл с EWM_updateMeta.
 * Текстовые значения пишем как UTF-8 и объявляем это в (0008,0005).
 */
inline void MatchDicomMediaStorage(DcmMetaInfo *meta, DcmDataset *dataset) {
  if (!dataset) {
    return;
  }
  dataset->putAndInsertString(DCM_SpecificCharacterSet, kDicomUtf8Charset);
  if (!meta) {
    return;
  }
  OFString sop_class;
  OFString sop_instance;
  dataset->findAndGetOFString(DCM_SOPClassUID, sop_class);
  dataset->findAndGetOFString(DCM_SOPInstanceUID, sop_instance);
  if (!sop_class.empty()) {
    meta->putAndInsertString(DCM_MediaStorageSOPClassUID, sop_class.c_str());
  }
  if (!sop_instance.empty()) {
    meta->putAndInsertString(DCM_MediaStorageSOPInstanceUID, sop_instance.c_str());
  }
}

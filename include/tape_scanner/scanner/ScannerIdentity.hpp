#pragma once

#include <tape_scanner/scanner/ScanGeometry.hpp>

#include <functional>
#include <string>

/**
 * Паспорт сканера из настроек приложения. Пустые поля в DICOM не пишутся.
 * acceptance_date хранится как DICOM DA (YYYYMMDD) либо пустая, если в настройках
 * даты нет или она не разбирается.
 */
struct ScannerIdentity {
  std::string serial;
  std::string acceptance_date;
  std::string accepted_by;
  std::string manufacturer;
  std::string model;
  std::string station_name;
  std::string institution;
  std::string software_version;
  std::string device_id;

  bool empty() const {
    return serial.empty() && acceptance_date.empty() && accepted_by.empty() && manufacturer.empty() && model.empty() &&
           station_name.empty() && institution.empty() && software_version.empty() && device_id.empty();
  }
};

/// section/key из settings.ini. Отсутствующий ключ — пустая строка, файл при этом не меняется.
using SettingsLookup = std::function<std::string(const std::string &section, const std::string &key)>;

/// YYYYMMDD, YYYY-MM-DD, YYYY.MM.DD или DD.MM.YYYY / DD-MM-YYYY. Иначе пустая строка.
std::string DicomAcceptanceDate(const std::string &text);

ScannerIdentity IdentityFromSettings(const SettingsLookup &lookup);

/// bin и базовый шаг pixel_um_x / pixel_um_y. В DICOM шаг умножается на bin по осям.
ScanGeometry GeometryFromSettings(const SettingsLookup &lookup);

/// Читает settings.ini. false — файл не открылся; geometry и identity тогда не меняются.
bool LoadAppScanSettingsFile(const std::string &path, ScanGeometry &geometry, ScannerIdentity &identity);

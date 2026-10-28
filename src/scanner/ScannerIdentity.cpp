#include <tape_scanner/scanner/ScannerIdentity.hpp>
#include <tape_scanner/scanner/Utils.hpp>
#include <tape_scanner/utils/ini.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace {

std::string Trim(std::string value) {
  auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };
  const auto begin = std::find_if(value.begin(), value.end(), not_space);
  const auto end = std::find_if(value.rbegin(), value.rend(), not_space).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

std::string CleanDicomText(std::string value, std::size_t max_len) {
  value = Trim(std::move(value));
  for (char &c : value) {
    if (c == '\\') {
      c = '/';
    }
  }
  if (value.size() > max_len) {
    value.resize(max_len);
    value = Trim(std::move(value));
  }
  return value;
}

bool ValidDate(int year, int month, int day) {
  if (year < 1900 || year > 9999 || month < 1 || month > 12 || day < 1) {
    return false;
  }
  static const int kDays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int days = kDays[month];
  const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
  if (month == 2 && leap) {
    days = 29;
  }
  return day <= days;
}

std::string FinishDate(int year, int month, int day) {
  if (!ValidDate(year, month, day)) {
    return {};
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d", year, month, day);
  return buf;
}

bool AllDigits(const std::string &text) {
  return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

} // namespace

std::string DicomAcceptanceDate(const std::string &text) {
  const std::string value = Trim(text);
  if (value.empty()) {
    return {};
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int consumed = 0;
  if (value.size() == 8 && AllDigits(value) &&
      std::sscanf(value.c_str(), "%4d%2d%2d%n", &year, &month, &day, &consumed) == 3 && consumed == 8) {
    return FinishDate(year, month, day);
  }

  char sep_a = 0;
  char sep_b = 0;
  consumed = 0;
  if (value.size() == 10 &&
      std::sscanf(value.c_str(), "%4d%c%2d%c%2d%n", &year, &sep_a, &month, &sep_b, &day, &consumed) == 5 &&
      consumed == 10 && (sep_a == '-' || sep_a == '.') && sep_a == sep_b) {
    return FinishDate(year, month, day);
  }
  consumed = 0;
  if (value.size() == 10 &&
      std::sscanf(value.c_str(), "%2d%c%2d%c%4d%n", &day, &sep_a, &month, &sep_b, &year, &consumed) == 5 &&
      consumed == 10 && (sep_a == '.' || sep_a == '-') && sep_a == sep_b) {
    return FinishDate(year, month, day);
  }
  return {};
}

ScannerIdentity IdentityFromSettings(const SettingsLookup &lookup) {
  ScannerIdentity identity;
  identity.serial = CleanDicomText(lookup("scanner", "serial"), 64);
  identity.acceptance_date = DicomAcceptanceDate(lookup("scanner", "acceptance_date"));
  identity.accepted_by = CleanDicomText(lookup("scanner", "accepted_by"), 64);
  identity.manufacturer = CleanDicomText(lookup("scanner", "manufacturer"), 64);
  identity.model = CleanDicomText(lookup("scanner", "model"), 64);
  identity.station_name = CleanDicomText(lookup("scanner", "station_name"), 16);
  identity.institution = CleanDicomText(lookup("scanner", "institution"), 64);
  identity.software_version = CleanDicomText(lookup("scanner", "software_version"), 64);
  identity.device_id = CleanDicomText(lookup("scanner", "device_id"), 64);
  return identity;
}

ScanGeometry GeometryFromSettings(const SettingsLookup &lookup) {
  ScanGeometry geometry;
  const std::string bin = Trim(lookup("scan", "bin"));
  if (!bin.empty()) {
    geometry.bin = safeStrToInt(bin);
  }
  const std::string um_x = Trim(lookup("scan", "pixel_um_x"));
  if (!um_x.empty()) {
    geometry.pixel_um_x = safeStrToDouble(um_x);
  }
  const std::string um_y = Trim(lookup("scan", "pixel_um_y"));
  if (!um_y.empty()) {
    geometry.pixel_um_y = safeStrToDouble(um_y);
  }
  return ScanGeometry::Normalized(geometry);
}

bool LoadAppScanSettingsFile(const std::string &path, ScanGeometry &geometry, ScannerIdentity &identity) {
  mINI::INIFile file(path);
  mINI::INIStructure ini;
  if (!file.read(ini)) {
    return false;
  }
  const SettingsLookup lookup = [&](const std::string &section, const std::string &key) {
    return ini.get(section).get(key);
  };
  geometry = GeometryFromSettings(lookup);
  identity = IdentityFromSettings(lookup);
  return true;
}

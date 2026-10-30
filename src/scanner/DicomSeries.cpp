#include <tape_scanner/scanner/DicomSeries.hpp>
#include <tape_scanner/scanner/DicomNumbers.hpp>
#include <tape_scanner/scanner/DicomScan.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcobject.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

namespace {

bool ReadPart(const std::filesystem::path &path, DicomSeriesPart &part, std::string &series_uid) {
  dcmIgnoreParsingErrors.set(OFTrue);
  dcmIgnoreFileMetaInformationGroupLength.set(OFTrue);
  DcmFileFormat file;
  // Элемент длиннее предела остаётся выгруженным. Пиксели (7FE0,0010) сегмента
  // больше этого порога, теги серии — нет.
  if (file.loadFile(path.string().c_str(), EXS_Unknown, EGL_noChange, static_cast<Uint32>(kDicomSeriesValueLimit))
          .bad()) {
    return false;
  }
  DcmDataset *dataset = file.getDataset();
  OFString series;
  OFString instance;
  OFString position_y;
  OFString spacing_row;
  Uint16 rows = 0;
  Uint16 cols = 0;
  if (dataset->findAndGetOFString(DCM_SeriesInstanceUID, series).bad()) {
    return false;
  }
  if (dataset->findAndGetOFString(DCM_InstanceNumber, instance).bad()) {
    return false;
  }
  if (dataset->findAndGetOFString(DCM_ImagePositionPatient, position_y, 1).bad()) {
    position_y = "0";
  }
  if (dataset->findAndGetOFString(DCM_PixelSpacing, spacing_row, 0).bad()) {
    spacing_row = "0";
  }
  dataset->findAndGetUint16(DCM_Rows, rows);
  dataset->findAndGetUint16(DCM_Columns, cols);

  series_uid = series.c_str();
  part.path = path.string();
  part.instance = std::atoi(instance.c_str());
  const auto position = ParseDicomDecimal(position_y.c_str());
  const auto spacing = ParseDicomDecimal(spacing_row.c_str());
  part.position_row_mm = position ? *position : 0;
  part.rows = rows;
  part.cols = cols;
  part.spacing_row_mm = spacing ? PixelSpacingToMm(*spacing) : 0;
  return true;
}

bool ConsiderDicomFile(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  for (char &ch : extension) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (extension == ".dcm" || extension == ".dicom") {
    return true;
  }
  return FileHasDicomMarker(path.string());
}

} // namespace

std::vector<DicomSeriesPart> ListDicomSeries(const std::string &any_segment_path) {
  std::vector<DicomSeriesPart> parts;
  const std::filesystem::path opened(any_segment_path);
  DicomSeriesPart current;
  std::string series_uid;
  if (!ReadPart(opened, current, series_uid)) {
    return parts;
  }

  const std::filesystem::path directory = opened.parent_path();
  std::error_code error;
  bool saw_opened = false;
  for (const auto &entry : std::filesystem::directory_iterator(directory, error)) {
    if (error || !entry.is_regular_file()) {
      continue;
    }
    if (!ConsiderDicomFile(entry.path())) {
      continue;
    }
    std::error_code same_error;
    if (std::filesystem::equivalent(opened, entry.path(), same_error) && !same_error) {
      parts.push_back(current);
      saw_opened = true;
      continue;
    }
    DicomSeriesPart part;
    std::string part_series;
    if (!ReadPart(entry.path(), part, part_series) || part_series != series_uid) {
      continue;
    }
    parts.push_back(std::move(part));
  }

  if (!saw_opened) {
    parts.push_back(std::move(current));
  }

  std::sort(parts.begin(), parts.end(), [](const DicomSeriesPart &a, const DicomSeriesPart &b) {
    if (a.position_row_mm != b.position_row_mm) {
      return a.position_row_mm < b.position_row_mm;
    }
    if (a.instance != b.instance) {
      return a.instance < b.instance;
    }
    return a.path < b.path;
  });
  return parts;
}

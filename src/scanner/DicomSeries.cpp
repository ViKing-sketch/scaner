#include <tape_scanner/scanner/DicomSeries.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

bool ReadPart(const std::filesystem::path &path, DicomSeriesPart &part, std::string &series_uid) {
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
  part.position_row_mm = std::strtod(position_y.c_str(), nullptr);
  part.rows = rows;
  part.cols = cols;
  part.spacing_row_mm = std::strtod(spacing_row.c_str(), nullptr);
  return true;
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
    if (entry.path().extension() != ".dcm" && entry.path().extension() != ".DCM") {
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

ImageScalePassport ReadImageScalePassport(const std::string &path) {
  ImageScalePassport passport;
  DcmFileFormat file;
  if (file.loadFile(path.c_str(), EXS_Unknown, EGL_noChange, static_cast<Uint32>(kDicomSeriesValueLimit)).bad()) {
    return passport;
  }
  DcmDataset *dataset = file.getDataset();
  OFString creator;
  if (dataset->findAndGetOFString(DcmTag(0x0019, 0x0010, EVR_LO), creator).bad() ||
      std::string(creator.c_str()) != "SCANER") {
    return passport;
  }
  OFString bin_text;
  if (dataset->findAndGetOFString(DcmTag(0x0019, 0x1001, EVR_IS), bin_text).bad()) {
    return passport;
  }
  const int bin = std::atoi(bin_text.c_str());
  if (bin != 1 && bin != 2) {
    return passport;
  }
  passport.present = true;
  passport.bin = bin;

  OFString plane;
  if (dataset->findAndGetOFString(DcmTag(0x0019, 0x1002, EVR_LO), plane).good()) {
    passport.plane = plane.c_str();
  }
  OFString roi;
  if (dataset->findAndGetOFStringArray(DcmTag(0x0019, 0x1003, EVR_LO), roi).good()) {
    passport.roi = roi.c_str();
  }
  OFString transform;
  if (dataset->findAndGetOFStringArray(DcmTag(0x0019, 0x1004, EVR_LO), transform).good()) {
    const std::string text = transform.c_str();
    const auto slash = text.find('\\');
    if (slash != std::string::npos) {
      passport.rotation_deg = std::atoi(text.c_str());
      passport.flip = std::atoi(text.c_str() + slash + 1);
    }
  }
  return passport;
}

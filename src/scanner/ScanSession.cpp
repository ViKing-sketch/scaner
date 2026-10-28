#include <tape_scanner/scanner/ScanSession.hpp>
#include <tape_scanner/scanner/Utils.hpp>

#include <tape_scanner/scanner/ScanLog.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcuid.h>

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace {

std::string MakeUid(const char *root) {
  char buf[100];
  return std::string(dcmGenerateUniqueIdentifier(buf, root));
}

} // namespace

std::string MakeDicomSegmentPath(const std::string &chosen_path, int instance_number) {
  const auto slash = chosen_path.find_last_of("/\\");
  const auto dot = chosen_path.find_last_of('.');
  const bool has_ext = dot != std::string::npos && (slash == std::string::npos || dot > slash);
  const std::string stem = has_ext ? chosen_path.substr(0, dot) : chosen_path;
  const std::string ext = has_ext ? chosen_path.substr(dot) : std::string(".dcm");
  char suffix[16];
  std::snprintf(suffix, sizeof(suffix), "_%03d", instance_number);
  return stem + suffix + ext;
}

ScanSession::ScanSession(std::string chosen_path, ScanGeometry geometry, int max_segment_rows)
    : m_chosen_path(std::move(chosen_path)), m_geometry(ScanGeometry::Normalized(geometry)),
      m_study_uid(MakeUid(SITE_STUDY_UID_ROOT)), m_series_uid(MakeUid(SITE_SERIES_UID_ROOT)),
      m_max_rows(max_segment_rows > 0 ? max_segment_rows : kMaxDicomDimension) {}

bool ScanSession::PushRegion(cv::Mat img, int x, int y) {
  if (img.empty() || img.type() != CV_16UC1) {
    ScanLogError("ScanSession: tile must be a non-empty CV_16UC1");
    return false;
  }
  if (img.rows > kMaxDicomDimension || img.cols > kMaxDicomDimension) {
    ScanLogErrorF("ScanSession: tile %ix%i exceeds the DICOM segment limit %i", img.cols, img.rows,
                  kMaxDicomDimension);
    return false;
  }
  if (x < 0 || y < 0 || x > kMaxDicomDimension) {
    ScanLogErrorF("ScanSession: tile origin (%i, %i) is outside the roll", x, y);
    return false;
  }
  if (x + img.cols > kMaxDicomDimension) {
    ScanLogErrorF("ScanSession: columns would exceed %i", kMaxDicomDimension);
    return false;
  }

  const bool same_stripe = m_open_abs_y >= 0 && y == m_open_abs_y && m_film.HasOpenStripe();
  if (!same_stripe && m_film.HasContent()) {
    if (m_split_at >= 0) {
      if (!CommitSplit()) {
        return false;
      }
    }
    const int bottom = m_film.ContentRows();
    const int projected = y + img.rows - m_y_origin;
    if (projected > m_max_rows) {
      const int local_top = y - m_y_origin;
      const int overlap = bottom - local_top;
      // Перекрытие сводится внутри полосы, поэтому режем после того,
      // как приедут остальные тайлы этой полосы. Без перекрытия файл
      // можно закрыть сразу: смешивать нечего.
      if (overlap > 0 && local_top >= 0) {
        m_split_at = bottom;
      } else if (!Flush()) {
        return false;
      }
    }
  }

  int local_y = y - m_y_origin;
  if (m_split_at < 0 && local_y < 0) {
    const int drop = -local_y;
    if (drop >= img.rows) {
      return true;
    }
    img = img.rowRange(drop, img.rows);
    local_y = 0;
  }
  if (m_split_at < 0 && local_y + img.rows > m_max_rows) {
    ScanLogErrorF("ScanSession: stripe would exceed the segment limit %i", m_max_rows);
    return false;
  }

  try {
    m_film.PushRegion(std::move(img), x, local_y);
  } catch (const std::exception &e) {
    ScanLogError(std::string("ScanSession: ") + e.what());
    return false;
  }
  m_open_abs_y = y;
  return true;
}

bool ScanSession::Finish() { return Flush(); }

bool ScanSession::HasOpenPixels() const { return m_film.HasContent() || m_split_at >= 0; }

void ScanSession::Discard() {
  m_split_at = -1;
  m_open_abs_y = -1;
  m_film.Reset();
}

bool ScanSession::WriteSegment(const cv::Mat &image) {
  if (image.empty() || image.rows > m_max_rows || image.cols > kMaxDicomDimension ||
      image.rows > kMaxDicomDimension) {
    ScanLogError("ScanSession: segment is empty or larger than one DICOM frame");
    return false;
  }

  DicomWriteParams params;
  params.study_instance_uid = m_study_uid;
  params.series_instance_uid = m_series_uid;
  params.instance_number = m_next_instance;
  params.pixel_spacing_row_mm = m_geometry.RowSpacingMm();
  params.pixel_spacing_col_mm = m_geometry.ColSpacingMm();
  params.image_position_row_mm = static_cast<double>(m_y_origin) * m_geometry.RowSpacingMm();

  const std::string path = MakeDicomSegmentPath(m_chosen_path, m_next_instance);
  if (!saveToDcm(image, path, params)) {
    ScanLogError("ScanSession: failed to save " + path);
    return false;
  }

  ScanLogMessageF("ScanSession: saved %s (%ix%i), instance %i", path.c_str(), image.cols, image.rows,
                  m_next_instance);
  m_paths.push_back(path);
  ++m_next_instance;
  ++m_saved;
  m_y_origin += image.rows;
  return true;
}

bool ScanSession::CommitSplit() {
  if (m_split_at < 0) {
    return true;
  }

  cv::Mat image = m_film.Render();
  if (image.empty() || m_split_at <= 0 || m_split_at > image.rows) {
    ScanLogError("ScanSession: split row is outside the rendered segment");
    return false;
  }

  cv::Mat head = image.rowRange(0, m_split_at).clone();
  cv::Mat tail;
  if (m_split_at < image.rows) {
    tail = image.rowRange(m_split_at, image.rows).clone();
  }
  if (!WriteSegment(head)) {
    return false;
  }

  m_split_at = -1;
  m_film.Reset();
  m_open_abs_y = -1;
  if (!tail.empty()) {
    try {
      m_film.PushRegion(std::move(tail), 0, 0);
    } catch (const std::exception &e) {
      ScanLogError(std::string("ScanSession: ") + e.what());
      return false;
    }
    m_open_abs_y = m_y_origin;
  }
  return true;
}

bool ScanSession::Flush() {
  if (m_split_at >= 0) {
    if (!CommitSplit()) {
      return false;
    }
  }
  if (!m_film.HasContent()) {
    return true;
  }

  cv::Mat image = m_film.Render();
  if (!WriteSegment(image)) {
    return false;
  }
  m_open_abs_y = -1;
  m_film.Reset();
  return true;
}

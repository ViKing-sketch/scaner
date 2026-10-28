#pragma once

#include <tape_scanner/scanner/Film.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/SessionStore.hpp>

#include <string>
#include <vector>

/**
 * Путь сегмента рядом с выбранным файлом: `film.dcm` -> `film_001.dcm`.
 * Номер экземпляра начинается с 1.
 */
std::string MakeDicomSegmentPath(const std::string &chosen_path, int instance_number);

/**
 * Один скан — одна Study UID и одна Series UID.
 * В RAM лежит только текущий сегмент плёнки. Когда следующая полоса
 * (граница шага роликов, не середина тайла) опустила бы нижний край
 * ниже предела строк, полоса дописывается до конца и сводится с хвостом
 * текущего холста тем же линейным переходом, что и внутри сегмента.
 * В файл уходит холст до низа до этой полосы: смешанное перекрытие
 * остаётся в нём, во следующем файле этих строк нет. Последний Finish()
 * хвост плёнки не обрезает.
 *
 * Каталог сеанса уникален и лежит рядом с выбранным файлом. Сам выбранный
 * файл и прежние имена stem_NNN.dcm в его каталоге не открываются и не
 * заменяются. Повтор того же base path получает новый каталог.
 * Статус Complete — только после проверки файлов по manifest.json.
 */
class ScanSession {
public:
  /// max_segment_rows оставлен параметром, чтобы тест стыка не строил холст в 60000 строк.
  ScanSession(std::string chosen_path, ScanGeometry geometry, int max_segment_rows = kMaxDicomDimension);

  /**
   * x, y — координаты тайла в сетке выбранного bin, от начала рулона.
   * Y внутри сегмента пересчитывается от начала этого сегмента, чтобы
   * холст не содержал пустые строки предыдущих сегментов.
   */
  bool PushRegion(cv::Mat img, int x, int y);

  /// Пишет хвост, если в нём есть пиксели. Пустой холст файл не создаёт и не затирает.
  bool Finish();

  int SavedSegmentCount() const { return m_saved; }
  const std::string &StudyUid() const { return m_study_uid; }
  const std::string &SeriesUid() const { return m_series_uid; }
  const ScanGeometry &Geometry() const { return m_geometry; }
  const std::vector<std::string> &SavedPaths() const { return m_paths; }
  const std::filesystem::path &Directory() const { return m_store.Directory(); }
  const SessionManifest &Manifest() const { return m_store.Manifest(); }
  SessionCommitStatus CommitStatus() const;
  static SessionRecovery Recover(const std::filesystem::path &session_dir) { return SessionStore::Recover(session_dir); }

private:
  struct PendingTile {
    cv::Mat image;
    int x = 0;
    int y = 0;
  };

  bool Flush();
  bool CommitSplit();
  bool WriteSegment(const cv::Mat &image);

  std::string m_chosen_path;
  ScanGeometry m_geometry;
  Film m_film;
  std::string m_study_uid;
  std::string m_series_uid;
  SessionStore m_store;
  int m_next_instance = 1;
  int m_y_origin = 0;
  int m_open_abs_y = -1;
  /// Локальная строка, по которой режется сегмент, когда переполняющая полоса ещё дописывается.
  int m_split_at = -1;
  int m_max_rows = kMaxDicomDimension;
  int m_saved = 0;
  std::vector<std::string> m_paths;
  std::vector<PendingTile> m_pending;
  std::vector<std::string> m_segment_tile_ids;
  std::vector<std::string> m_carried_tile_ids;
  std::vector<std::string> m_last_segment_tile_ids;
};

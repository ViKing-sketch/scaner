#pragma once

#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/Utils.hpp>

#include <opencv2/core.hpp>

#include <filesystem>
#include <string>
#include <vector>

/**
 * Каталог одного сеанса, формат tape-session версии 1.
 *
 *   <каталог выбранного файла>/<stem>.session-<id>/
 *     manifest.json
 *     journal.jsonl
 *     tiles/tNNNN.tif          исходный тайл, TIFF Mono16 без сжатия
 *     segments/<stem>_NNN.dcm  производный сегмент Secondary Capture
 *
 * Подтверждённым считается только файл, чей относительный путь и sha256
 * записаны в manifest.json. Строка журнала и файл, которого нет в manifest,
 * сеанс не завершают. Статус complete ставится после проверки каждого
 * записанного пути и хеша.
 *
 * Временный файл пишется рядом с итоговым, fsync, проверка чтения, затем
 * публикация имени без замены существующего файла и fsync каталога.
 * manifest той же сессии обновляется так же, но своё предыдущее имя заменить может.
 * rename не объявляется единственной защитой от потери питания.
 *
 * Маска валидных пикселей и план ожидаемых тайлов здесь не создаются.
 * Один BigTIFF на всё исследование не пишется.
 * Текст про fsync относится к локальной ФС, которая его выполняет, и не
 * является результатом проверки диска сканера.
 */

inline constexpr int kSessionFormatVersion = 1;
inline constexpr const char *kSessionFormatName = "tape-session";

enum class SessionCommitStatus { Empty, Open, Partial, Complete, Fault };

struct SessionTileRecord {
  std::string tile_id;
  std::string relative_path;
  std::string sha256;
  int x = 0;
  int y = 0;
  int rows = 0;
  int cols = 0;
};

struct SessionSegmentRecord {
  int instance = 0;
  std::string relative_path;
  std::string sha256;
  std::string sop_uid;
  int rows = 0;
  int cols = 0;
  int y_origin = 0;
  std::vector<std::string> source_tile_ids;
};

struct SessionManifest {
  int format_version = kSessionFormatVersion;
  std::string session_id;
  SessionCommitStatus status = SessionCommitStatus::Open;
  std::string base_path;
  std::string study_uid;
  std::string series_uid;
  int bin = kDefaultBin;
  double pixel_um_x = kNominalPixelUm;
  double pixel_um_y = kNominalPixelUm;
  std::string calibration_ref;
  std::vector<SessionTileRecord> tiles;
  std::vector<SessionSegmentRecord> segments;
};

struct SessionRecovery {
  bool manifest_read = false;
  SessionCommitStatus status = SessionCommitStatus::Fault;
  int confirmed_tiles = 0;
  int confirmed_segments = 0;
  int orphan_files = 0;
  int broken_records = 0;
  SessionManifest manifest;
};

std::string MakeSessionId();

std::string Sha256Hex(const void *data, size_t size);

std::string Sha256FileHex(const std::filesystem::path &path);

const char *SessionStatusName(SessionCommitStatus status);

bool SessionStatusFromName(const std::string &name, SessionCommitStatus &status);

bool ReadMono16Tiff(const std::filesystem::path &path, cv::Mat &image);

bool ReadSessionManifest(const std::filesystem::path &manifest_path, SessionManifest &manifest);

class SessionStore {
public:
  SessionStore(std::filesystem::path chosen_path, std::string session_id, ScanGeometry geometry,
               std::string study_uid, std::string series_uid);

  const std::filesystem::path &ChosenPath() const { return m_chosen; }
  const std::filesystem::path &Directory() const { return m_directory; }
  const SessionManifest &Manifest() const { return m_manifest; }
  bool IsOpen() const { return m_ready; }

  /// Создаёт новый каталог. Существующий каталог, выбранный файл и
  /// legacy-имя stem_NNN.dcm не открываются и не заменяются.
  bool EnsureDirectory();

  bool CommitTile(const cv::Mat &image, int x, int y, std::string &tile_id);

  /// Пишет сегмент во временный файл и добавляет его в manifest только после
  /// публикации. Повтор с тем же номером принимает уже совпавший файл и не
  /// перезаписывает другой.
  bool CommitSegment(const cv::Mat &image, const DicomWriteParams &params, int y_origin,
                     const std::vector<std::string> &source_tile_ids, std::string &out_path);

  bool HashesMatch() const;

  bool SetStatus(SessionCommitStatus status);

  void TrySetStatus(SessionCommitStatus status) { SetStatus(status); }

  static SessionRecovery Recover(const std::filesystem::path &session_dir);

private:
  bool WriteManifest();
  bool AppendJournal(const std::string &object_json);
  std::string SegmentRelative(int instance) const;
  bool RememberFile(const std::filesystem::path &final);

  std::filesystem::path m_chosen;
  std::filesystem::path m_directory;
  std::string m_stem;
  SessionManifest m_manifest;
  bool m_ready = false;
};

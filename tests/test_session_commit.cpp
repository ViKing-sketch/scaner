#include <tape_scanner/scanner/ScanSession.hpp>
#include <tape_scanner/scanner/SessionStore.hpp>
#include <tape_scanner/scanner/Utils.hpp>

#include <opencv2/core.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool ok, const char *expr, int line) {
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

std::filesystem::path Scratch(const char *name) {
  const auto dir = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

std::string ReadFile(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void WriteBytes(const std::filesystem::path &path, const std::string &bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << bytes;
}

ScanGeometry NominalGeometry() {
  ScanGeometry geometry;
  geometry.bin = 2;
  geometry.pixel_um_x = kNominalPixelUm;
  geometry.pixel_um_y = kNominalPixelUm;
  return geometry;
}

void TestSha256AndTiff() {
  CHECK(Sha256Hex("", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  const char abc[] = "abc";
  CHECK(Sha256Hex(abc, 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  const auto dir = Scratch("tape_scanner_z06_tiff");
  SessionStore store(dir / "roll.dcm", MakeSessionId(), NominalGeometry(), "1.2.3.4.5.6.7.61", "1.2.3.4.5.6.7.62");
  cv::Mat tile(2, 5, CV_16UC1);
  for (int col = 0; col < tile.cols; ++col) {
    tile.at<uint16_t>(0, col) = static_cast<uint16_t>(col);
    tile.at<uint16_t>(1, col) = static_cast<uint16_t>(1000 + col);
  }
  std::string tile_id;
  CHECK(store.CommitTile(tile, 3, 4, tile_id));
  CHECK(tile_id == "t0001");
  cv::Mat roundtrip;
  CHECK(ReadMono16Tiff(store.Directory() / "tiles" / "t0001.tif", roundtrip));
  CHECK(roundtrip.rows == 2);
  CHECK(roundtrip.cols == 5);
  CHECK(roundtrip.at<uint16_t>(1, 4) == 1004);
  CHECK(store.Manifest().tiles.size() == 1);
  CHECK(store.Manifest().tiles[0].x == 3);
  CHECK(store.Manifest().tiles[0].y == 4);
  CHECK(store.Manifest().status == SessionCommitStatus::Open);
  std::filesystem::remove_all(dir);
}

void TestSameBasePathDoesNotOverwrite() {
  const auto dir = Scratch("tape_scanner_z06_names");
  const auto chosen = dir / "roll.dcm";
  const std::string keep = "chosen-file-stays";
  const std::string legacy = "legacy-segment-stays";
  WriteBytes(chosen, keep);
  WriteBytes(dir / "roll_001.dcm", legacy);

  ScanGeometry geometry = NominalGeometry();
  ScanSession first(chosen.string(), geometry);
  cv::Mat pixels(8, 6, CV_16UC1, cv::Scalar(1111));
  CHECK(first.PushRegion(pixels, 0, 0));
  CHECK(first.Finish());
  CHECK(first.CommitStatus() == SessionCommitStatus::Complete);
  CHECK(first.SavedPaths().size() == 1);
  CHECK(ReadFile(chosen) == keep);
  CHECK(ReadFile(dir / "roll_001.dcm") == legacy);
  CHECK(first.SavedPaths()[0] != (dir / "roll_001.dcm").string());
  const auto first_bytes = ReadFile(first.SavedPaths()[0]);
  const auto first_dir = first.Directory();

  ScanSession second(chosen.string(), geometry);
  cv::Mat other(8, 6, CV_16UC1, cv::Scalar(2222));
  CHECK(second.PushRegion(other, 0, 0));
  CHECK(second.Finish());
  CHECK(second.Directory() != first_dir);
  CHECK(second.CommitStatus() == SessionCommitStatus::Complete);
  CHECK(ReadFile(first.SavedPaths()[0]) == first_bytes);
  CHECK(ReadFile(chosen) == keep);
  CHECK(ReadFile(dir / "roll_001.dcm") == legacy);

  SessionManifest manifest;
  CHECK(ReadSessionManifest(first_dir / "manifest.json", manifest));
  CHECK(manifest.status == SessionCommitStatus::Complete);
  CHECK(manifest.format_version == 1);
  CHECK(manifest.segments.size() == 1);
  CHECK(manifest.tiles.size() == 1);
  CHECK(manifest.calibration_ref.empty());
  const std::string text = ReadFile(first_dir / "manifest.json");
  CHECK(text.find("Rename is not the power-loss barrier.") != std::string::npos);
  CHECK(text.find("valid_pixel_mask") != std::string::npos);
  CHECK(text.find("not a measured result on a scanner PC") != std::string::npos);

  SessionStore again(chosen, manifest.session_id, geometry, manifest.study_uid, manifest.series_uid);
  std::string ignored;
  CHECK(!again.CommitTile(other, 0, 0, ignored));
  CHECK(ReadFile(first.SavedPaths()[0]) == first_bytes);
  std::filesystem::remove_all(dir);
}

void TestOverwriteRefused() {
  const auto dir = Scratch("tape_scanner_z06_overwrite");
  const auto path = dir / "frame.dcm";
  cv::Mat first(2, 2, CV_16UC1, cv::Scalar(7));
  cv::Mat second(2, 2, CV_16UC1, cv::Scalar(9));
  DicomWriteParams params;
  params.instance_number = 1;
  CHECK(saveToDcm(first, path.string(), params));
  const auto before = ReadFile(path);
  CHECK(!saveToDcm(second, path.string(), params));
  CHECK(ReadFile(path) == before);
  CHECK(!std::filesystem::exists(dir / "frame.dcm.partial"));
  std::filesystem::remove_all(dir);
}

void TestWriteRefusalKeepsConfirmedSegment() {
  const auto dir = Scratch("tape_scanner_z06_disk");
  const auto chosen = dir / "roll.dcm";
  ScanGeometry geometry = NominalGeometry();
  SessionStore store(chosen, MakeSessionId(), geometry, "1.2.3.4.5.6.7.71", "1.2.3.4.5.6.7.72");
  cv::Mat tile(8, 4, CV_16UC1, cv::Scalar(1000));
  std::string tile_id;
  CHECK(store.CommitTile(tile, 0, 0, tile_id));
  DicomWriteParams params;
  params.study_instance_uid = "1.2.3.4.5.6.7.71";
  params.series_instance_uid = "1.2.3.4.5.6.7.72";
  params.instance_number = 1;
  params.pixel_spacing_row_mm = geometry.RowSpacingMm();
  params.pixel_spacing_col_mm = geometry.ColSpacingMm();
  std::string path;
  CHECK(store.CommitSegment(tile, params, 0, {tile_id}, path));
  CHECK(store.Manifest().status == SessionCommitStatus::Open);
  const auto confirmed = ReadFile(path);

  const auto segments = store.Directory() / "segments";
  {
    std::filesystem::permissions(segments, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    struct Restore {
      std::filesystem::path path;
      ~Restore() {
        std::error_code error;
        std::filesystem::permissions(path, std::filesystem::perms::owner_all, std::filesystem::perm_options::add, error);
      }
    } restore{segments};

    cv::Mat next(8, 4, CV_16UC1, cv::Scalar(2000));
    DicomWriteParams second = params;
    second.instance_number = 2;
    second.image_position_row_mm = 8 * geometry.RowSpacingMm();
    std::string second_path;
    CHECK(!store.CommitSegment(next, second, 8, {}, second_path));
    CHECK(store.Manifest().status != SessionCommitStatus::Complete);
    CHECK(store.Manifest().segments.size() == 1);
    CHECK(ReadFile(path) == confirmed);

    const SessionRecovery recovery = SessionStore::Recover(store.Directory());
    CHECK(recovery.manifest_read);
    CHECK(recovery.confirmed_segments == 1);
    CHECK(recovery.broken_records == 0);
    CHECK(recovery.status != SessionCommitStatus::Complete);
  }
  std::filesystem::remove_all(dir);
}

void TestCrashBeforeManifestIsNotComplete() {
  const auto dir = Scratch("tape_scanner_z06_crash");
  const auto chosen = dir / "roll.dcm";
  ScanGeometry geometry = NominalGeometry();
  SessionStore store(chosen, MakeSessionId(), geometry, "1.2.3.4.5.6.7.81", "1.2.3.4.5.6.7.82");
  cv::Mat tile(8, 4, CV_16UC1, cv::Scalar(5));
  std::string tile_id;
  CHECK(store.CommitTile(tile, 0, 0, tile_id));
  DicomWriteParams params;
  params.study_instance_uid = "1.2.3.4.5.6.7.81";
  params.series_instance_uid = "1.2.3.4.5.6.7.82";
  params.instance_number = 1;
  params.pixel_spacing_row_mm = geometry.RowSpacingMm();
  params.pixel_spacing_col_mm = geometry.ColSpacingMm();
  std::string path;
  CHECK(store.CommitSegment(tile, params, 0, {tile_id}, path));
  CHECK(store.Manifest().segments.size() == 1);
  CHECK(store.Manifest().status == SessionCommitStatus::Open);

  DicomWriteParams orphan_params = params;
  orphan_params.instance_number = 2;
  orphan_params.image_position_row_mm = 8 * geometry.RowSpacingMm();
  cv::Mat orphan_image(8, 4, CV_16UC1, cv::Scalar(9));
  const auto orphan = store.Directory() / "segments" / "roll_002.dcm";
  CHECK(saveToDcm(orphan_image, orphan.string(), orphan_params));
  WriteBytes(store.Directory() / "segments" / "partial.dcm.partial", "not-a-dicom");

  const SessionRecovery recovery = SessionStore::Recover(store.Directory());
  CHECK(recovery.manifest_read);
  CHECK(recovery.status == SessionCommitStatus::Open);
  CHECK(recovery.status != SessionCommitStatus::Complete);
  CHECK(recovery.confirmed_segments == 1);
  CHECK(recovery.confirmed_tiles == 1);
  CHECK(recovery.orphan_files == 1);
  CHECK(recovery.broken_records == 0);
  CHECK(recovery.manifest.segments.size() == 1);
  CHECK(recovery.manifest.segments[0].relative_path.find("roll_001.dcm") != std::string::npos);

  SessionManifest on_disk;
  CHECK(ReadSessionManifest(store.Directory() / "manifest.json", on_disk));
  CHECK(on_disk.status == SessionCommitStatus::Open);
  CHECK(on_disk.segments.size() == 1);
  std::filesystem::remove_all(dir);
}

void TestBrokenCompleteIsDowngraded() {
  const auto dir = Scratch("tape_scanner_z06_recover");
  const auto chosen = dir / "roll.dcm";
  ScanGeometry geometry = NominalGeometry();
  ScanSession session(chosen.string(), geometry, 25);
  cv::Mat head(20, 4, CV_16UC1, cv::Scalar(1000));
  cv::Mat next(20, 4, CV_16UC1, cv::Scalar(2000));
  CHECK(session.PushRegion(head, 0, 0));
  CHECK(session.PushRegion(next, 0, 10));
  CHECK(session.Finish());
  CHECK(session.SavedSegmentCount() == 2);
  CHECK(session.CommitStatus() == SessionCommitStatus::Complete);
  CHECK(session.Manifest().segments[1].source_tile_ids.size() >= 1);
  const auto first_bytes = ReadFile(session.SavedPaths()[0]);
  const auto session_id = session.Manifest().session_id;

  std::filesystem::remove(session.SavedPaths()[1]);
  const SessionRecovery missing = ScanSession::Recover(session.Directory());
  CHECK(missing.manifest_read);
  CHECK(missing.status == SessionCommitStatus::Fault);
  CHECK(missing.confirmed_segments == 1);
  CHECK(missing.broken_records == 1);
  CHECK(ReadFile(session.SavedPaths()[0]) == first_bytes);
  SessionManifest manifest;
  CHECK(ReadSessionManifest(session.Directory() / "manifest.json", manifest));
  CHECK(manifest.status == SessionCommitStatus::Fault);
  CHECK(manifest.session_id == session_id);
  CHECK(manifest.segments.size() == 2);

  std::filesystem::remove_all(dir);

  const auto flip_dir = Scratch("tape_scanner_z06_flip");
  ScanSession intact((flip_dir / "roll.dcm").string(), geometry);
  cv::Mat frame(6, 6, CV_16UC1, cv::Scalar(42));
  CHECK(intact.PushRegion(frame, 0, 0));
  CHECK(intact.Finish());
  CHECK(intact.CommitStatus() == SessionCommitStatus::Complete);
  {
    std::string bytes = ReadFile(intact.SavedPaths()[0]);
    CHECK(!bytes.empty());
    bytes.back() = static_cast<char>(static_cast<unsigned char>(bytes.back()) ^ 0xff);
    WriteBytes(intact.SavedPaths()[0], bytes);
  }
  const SessionRecovery mismatched = ScanSession::Recover(intact.Directory());
  CHECK(mismatched.status == SessionCommitStatus::Fault);
  CHECK(mismatched.broken_records >= 1);
  CHECK(mismatched.manifest.status == SessionCommitStatus::Fault);
  SessionManifest flipped_manifest;
  CHECK(ReadSessionManifest(intact.Directory() / "manifest.json", flipped_manifest));
  CHECK(flipped_manifest.status != SessionCommitStatus::Complete);
  std::filesystem::remove_all(flip_dir);
}

} // namespace

int main() {
  TestSha256AndTiff();
  TestSameBasePathDoesNotOverwrite();
  TestOverwriteRefused();
  TestWriteRefusalKeepsConfirmedSegment();
  TestCrashBeforeManifestIsNotComplete();
  TestBrokenCompleteIsDowngraded();

  if (g_failures != 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "test_session_commit ok\n";
  return 0;
}

#include <tape_scanner/scanner/LinkLoss.hpp>
#include <tape_scanner/scanner/ScanGeometry.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>

#include <opencv2/core.hpp>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool ok, const char *expr, int line) {
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

std::filesystem::path MakeTempDir() {
  const auto root = std::filesystem::temp_directory_path() / "scaner-link-loss";
  std::filesystem::create_directories(root);
  const auto dir = root / std::to_string(std::filesystem::hash_value(root) ^ static_cast<size_t>(g_failures + 17));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

cv::Mat Tile() {
  cv::Mat tile(4, 4, CV_16UC1, cv::Scalar(1000));
  return tile;
}

} // namespace

int main() {
  const auto defaults = LinkLossSettingsFromText("", "");
  CHECK(defaults.hold_motors);
  CHECK(defaults.save_captured);

  const auto typed = LinkLossSettingsFromText("0", "false");
  CHECK(!typed.hold_motors);
  CHECK(!typed.save_captured);

  const auto words = LinkLossSettingsFromText(" yes ", "ON");
  CHECK(words.hold_motors);
  CHECK(words.save_captured);

  const auto garbage = LinkLossSettingsFromText("maybe", "2");
  CHECK(garbage.hold_motors);
  CHECK(garbage.save_captured);

  const LinkEdge first_miss = AdvanceLink(LinkPhase::Unknown, false);
  CHECK(first_miss.next == LinkPhase::Down);
  CHECK(!first_miss.became_lost);

  const LinkEdge up = AdvanceLink(first_miss.next, true);
  CHECK(up.next == LinkPhase::Up);
  CHECK(up.became_up);

  const LinkEdge lost = AdvanceLink(up.next, false);
  CHECK(lost.next == LinkPhase::Reconnecting);
  CHECK(lost.became_lost);

  const LinkEdge still = AdvanceLink(lost.next, false);
  CHECK(still.next == LinkPhase::Reconnecting);
  CHECK(!still.became_lost);

  const LinkEdge back = AdvanceLink(still.next, true);
  CHECK(back.next == LinkPhase::Up);
  CHECK(back.became_up);
  CHECK(!back.became_lost);

  LinkLossNote quiet;
  const std::string idle = FormatLinkStatus(LinkPhase::Up, LinkPhase::Down, quiet);
  CHECK(idle == "Camera connected. Controller disconnected.");

  LinkLossNote held;
  held.visible = true;
  held.scan_stopped = true;
  held.mention_capture = true;
  held.motors = MotorLossAction::Hold;
  held.save_captured = true;
  held.save_ok = true;
  held.saved_segments = 2;
  const std::string held_line = FormatLinkStatus(LinkPhase::Reconnecting, LinkPhase::Up, held);
  CHECK(held_line.find("Connecting to camera..") != std::string::npos);
  CHECK(held_line.find("Controller connected") != std::string::npos);
  CHECK(held_line.find("Link lost. Scan stopped.") != std::string::npos);
  CHECK(held_line.find("Motors left in place.") != std::string::npos);
  CHECK(held_line.find("Captured frames kept (2).") != std::string::npos);
  CHECK(held_line.find("Motors returning home") == std::string::npos);

  LinkLossNote home;
  home.visible = true;
  home.mention_capture = true;
  home.motors = MotorLossAction::Home;
  home.save_captured = false;
  const std::string home_line = FormatLinkStatus(LinkPhase::Up, LinkPhase::Reconnecting, home);
  CHECK(home_line.find("Motors returning home.") != std::string::npos);
  CHECK(home_line.find("Open scan not saved.") != std::string::npos);
  CHECK(home_line.find("Link lost. Scan stopped.") == std::string::npos);

  const auto dir = MakeTempDir();
  const std::string chosen = (dir / "film.dcm").string();
  ScanGeometry geometry;
  geometry.bin = 1;

  {
    ScanSession session(chosen, geometry, 100);
    CHECK(session.PushRegion(Tile(), 0, 0));
    CHECK(session.HasOpenPixels());
    const auto applied = ApplyLinkLoss(&session, LinkLossSettingsFromText("1", "1"));
    CHECK(applied.motors == MotorLossAction::Hold);
    CHECK(applied.save_captured);
    CHECK(applied.save_ok);
    CHECK(applied.saved_segments == 1);
    CHECK(!applied.discarded);
    CHECK(!session.HasOpenPixels());
    CHECK(std::filesystem::exists(session.SavedPaths().at(0)));
  }

  {
    ScanSession session(chosen, geometry, 100);
    CHECK(session.PushRegion(Tile(), 0, 0));
    int files_before = 0;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file()) {
        ++files_before;
      }
    }
    const auto applied = ApplyLinkLoss(&session, LinkLossSettingsFromText("0", "0"));
    CHECK(applied.motors == MotorLossAction::Home);
    CHECK(!applied.save_captured);
    CHECK(applied.discarded);
    CHECK(applied.saved_segments == 0);
    CHECK(!session.HasOpenPixels());
    int files_after = 0;
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
      if (entry.is_regular_file()) {
        ++files_after;
      }
    }
    CHECK(files_after == files_before);
  }

  {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    ScanSession session(chosen, geometry, 100);
    CHECK(session.PushRegion(Tile(), 0, 0));
    CHECK(ApplyLinkLoss(&session, LinkLossSettingsFromText("1", "1")).save_ok);
    const std::string kept = session.SavedPaths().at(0);
    CHECK(session.PushRegion(Tile(), 0, 8));
    CHECK(session.HasOpenPixels());
    const auto applied = ApplyLinkLoss(&session, LinkLossSettingsFromText("1", "0"));
    CHECK(applied.discarded);
    CHECK(applied.saved_segments == 1);
    CHECK(std::filesystem::exists(kept));
    CHECK(!session.HasOpenPixels());
  }

  std::filesystem::remove_all(dir);

  if (g_failures != 0) {
    std::cerr << g_failures << " link-loss checks failed\n";
    return 1;
  }
  std::cout << "link-loss checks passed\n";
  return 0;
}

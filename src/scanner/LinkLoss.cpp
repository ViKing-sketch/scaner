#include <tape_scanner/scanner/LinkLoss.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>

#include <cctype>

namespace {

std::string TrimmedLower(std::string text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  text = text.substr(begin, end - begin + 1);
  for (char &ch : text) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return text;
}

const char *PhasePhrase(LinkPhase phase, const char *up, const char *reconnecting, const char *down) {
  switch (phase) {
  case LinkPhase::Up:
    return up;
  case LinkPhase::Reconnecting:
    return reconnecting;
  case LinkPhase::Down:
  case LinkPhase::Unknown:
    return down;
  }
  return down;
}

} // namespace

bool ParseIniBool(const std::string &text, bool fallback) {
  const std::string value = TrimmedLower(text);
  if (value.empty()) {
    return fallback;
  }
  if (value == "1" || value == "true" || value == "yes" || value == "on") {
    return true;
  }
  if (value == "0" || value == "false" || value == "no" || value == "off") {
    return false;
  }
  return fallback;
}

LinkLossSettings LinkLossSettingsFromText(const std::string &hold_motors, const std::string &save_captured) {
  LinkLossSettings settings;
  settings.hold_motors = ParseIniBool(hold_motors, true);
  settings.save_captured = ParseIniBool(save_captured, true);
  return settings;
}

LinkEdge AdvanceLink(LinkPhase previous, bool up) {
  LinkEdge edge;
  if (up) {
    edge.next = LinkPhase::Up;
    edge.became_up = previous != LinkPhase::Up;
    return edge;
  }

  if (previous == LinkPhase::Up) {
    edge.next = LinkPhase::Reconnecting;
    edge.became_lost = true;
    return edge;
  }
  if (previous == LinkPhase::Reconnecting) {
    edge.next = LinkPhase::Reconnecting;
    return edge;
  }
  edge.next = LinkPhase::Down;
  return edge;
}

const char *CameraLinkPhrase(LinkPhase phase) {
  return PhasePhrase(phase, "Camera connected", "Connecting to camera..", "Camera disconnected");
}

const char *ControllerLinkPhrase(LinkPhase phase) {
  return PhasePhrase(phase, "Controller connected", "Connecting to controller..", "Controller disconnected");
}

std::string FormatLinkStatus(LinkPhase camera, LinkPhase controller, const LinkLossNote &note) {
  std::string line = std::string(CameraLinkPhrase(camera)) + ". " + ControllerLinkPhrase(controller) + ".";
  if (!note.visible) {
    return line;
  }
  if (note.scan_stopped) {
    line += " Link lost. Scan stopped.";
  }
  line += note.motors == MotorLossAction::Hold ? " Motors left in place." : " Motors returning home.";
  if (!note.mention_capture) {
    return line;
  }
  if (!note.save_captured) {
    line += " Open scan not saved.";
  } else if (!note.save_ok) {
    line += " Failed to save captured frames.";
  } else if (note.saved_segments > 0) {
    line += " Captured frames kept (" + std::to_string(note.saved_segments) + ").";
  } else {
    line += " Captured frames kept.";
  }
  return line;
}

LinkLossResult ApplyLinkLoss(ScanSession *session, const LinkLossSettings &settings) {
  LinkLossResult result;
  result.motors = settings.hold_motors ? MotorLossAction::Hold : MotorLossAction::Home;
  result.save_captured = settings.save_captured;
  if (!session) {
    return result;
  }

  if (!settings.save_captured) {
    session->Discard();
    result.discarded = true;
    result.saved_segments = session->SavedSegmentCount();
    return result;
  }

  result.save_ok = session->Finish();
  result.saved_segments = session->SavedSegmentCount();
  return result;
}

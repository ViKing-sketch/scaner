#include <tape_scanner/scanner/ExposurePulse.hpp>
#include <tape_scanner/scanner/ScanLog.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utility>

namespace {

std::string FormatUs(double value) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.6g", value);
  return buf;
}

std::string JsonEscape(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

} // namespace

bool ControllerPulseSimulator::CommandLightPulse(double duration_us) {
  if (!std::isfinite(duration_us) || !(duration_us > 0.0)) {
    ScanLogError("Controller simulator: light pulse duration must be positive");
    return false;
  }
  m_pulses.push_back(duration_us);
  ScanLogMessageF("Controller simulator: light pulse %.6g us", duration_us);
  return true;
}

ExposureShot PlanExposure(double target_us, double margin_us) {
  ExposureShot shot;
  shot.target_us = target_us;
  shot.margin_us = margin_us;
  if (!std::isfinite(target_us) || !(target_us > 0.0) || !std::isfinite(margin_us) || !(margin_us >= 0.0)) {
    ScanLogError("Exposure: pulse target must be positive and shutter margin must be >= 0");
    return shot;
  }

  shot.pulse_us = target_us;
  shot.shutter_us = target_us + margin_us;
  if (!(shot.shutter_us >= shot.pulse_us)) {
    ScanLogError("Exposure: camera shutter does not cover the light pulse");
    return shot;
  }

  shot.planned = true;
  ScanLogMessageF("Exposure: pulse %.6g us (effective), camera shutter %.6g us (margin %.6g us). Shutter covers the pulse.",
                  shot.pulse_us, shot.shutter_us, shot.margin_us);
  return shot;
}

ExposureShot CommandExposure(IControllerPulse &controller, double target_us, double margin_us) {
  ExposureShot shot = PlanExposure(target_us, margin_us);
  if (!shot.planned) {
    return shot;
  }
  shot.pulse_commanded = controller.CommandLightPulse(shot.pulse_us);
  if (!shot.pulse_commanded) {
    ScanLogMessageF("Exposure: controller did not accept pulse %.6g us. TODO(Z04): host framing is not sent yet.",
                    shot.pulse_us);
  }
  return shot;
}

std::string FormatExposureSessionComment(const SessionExposureMetadata &metadata) {
  std::string effective;
  std::string shutter;
  for (size_t i = 0; i < metadata.exposures.size(); ++i) {
    if (i != 0) {
      effective += ',';
      shutter += ',';
    }
    effective += FormatUs(metadata.exposures[i].pulse_us);
    shutter += FormatUs(metadata.exposures[i].shutter_us);
  }

  std::string text = metadata.note.empty() ? std::string(kExposurePulseNote) : metadata.note;
  text += " effective_us=";
  text += effective;
  text += " shutter_us=";
  text += shutter;
  constexpr size_t kImageCommentLimit = 1024;
  if (text.size() > kImageCommentLimit) {
    text.resize(kImageCommentLimit);
    ScanLogError("Exposure comment truncated to 1024 characters");
  }
  return text;
}

bool WriteExposureMetadataFile(const std::string &path, const SessionExposureMetadata &metadata) {
  const std::string tmp = path + ".tmp";
  std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
  if (!out) {
    ScanLogError("Exposure metadata: cannot write " + tmp);
    return false;
  }

  const std::string note = metadata.note.empty() ? std::string(kExposurePulseNote) : metadata.note;
  out << "{\n";
  out << "  \"exposure_model\": \"" << JsonEscape(metadata.model) << "\",\n";
  out << "  \"exposure_note\": \"" << JsonEscape(note) << "\",\n";
  out << "  \"exposures\": [\n";
  for (size_t i = 0; i < metadata.exposures.size(); ++i) {
    const ExposureShot &shot = metadata.exposures[i];
    out << "    {\"target_us\": " << FormatUs(shot.target_us) << ", \"pulse_us\": " << FormatUs(shot.pulse_us)
        << ", \"effective_us\": " << FormatUs(shot.pulse_us) << ", \"shutter_us\": " << FormatUs(shot.shutter_us)
        << ", \"margin_us\": " << FormatUs(shot.margin_us)
        << ", \"pulse_commanded\": " << (shot.pulse_commanded ? "true" : "false") << "}";
    if (i + 1 != metadata.exposures.size()) {
      out << ",";
    }
    out << "\n";
  }
  out << "  ]\n";
  out << "}\n";
  out.close();
  if (!out) {
    ScanLogError("Exposure metadata: write failed " + tmp);
    return false;
  }

  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    ScanLogError("Exposure metadata: cannot publish " + path + ": " + ec.message());
    return false;
  }
  ScanLogMessage("Exposure metadata written to " + path);
  return true;
}

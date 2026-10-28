#include <tape_scanner/scanner/ExposureSteps.hpp>
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

/**
 * PLACEHOLDER: awaiting Max's measured exposure step table. Not a calibration.
 *
 * Единственная наблюдённая строка — пример Макса:
 *   запрос в закрытом интервале [40, 69] мкс даёт кадр как 70 мкс.
 *
 * Остальные строки — границы-заглушки. Их effective_us — круглые
 * подставные числа, не измеренная доза. Когда Макс пришлёт таблицу,
 * заменить её целиком. Не подгонять эти числа и не считать скан,
 * взвешенный этой таблицей, откалиброванным.
 *
 * Запрос выше последней строки не зажимается в выдуманный шаг.
 */
std::vector<ExposureStep> PlaceholderRows() {
  return {
      {0.0, 39.0, 20.0, ExposureStepOrigin::Placeholder},
      {40.0, 69.0, 70.0, ExposureStepOrigin::ObservedExample},
      {70.0, 139.0, 140.0, ExposureStepOrigin::Placeholder},
      {140.0, 399.0, 400.0, ExposureStepOrigin::Placeholder},
  };
}

} // namespace

ExposureStepTable::ExposureStepTable(std::vector<ExposureStep> steps, std::string note, bool placeholder)
    : m_note(std::move(note)), m_placeholder(placeholder) {
  for (const ExposureStep &step : steps) {
    if (!std::isfinite(step.requested_min_us) || !std::isfinite(step.requested_max_us) ||
        !std::isfinite(step.effective_us) || !(step.effective_us > 0.0) ||
        step.requested_min_us > step.requested_max_us) {
      ScanLogError("Exposure step dropped: need finite bounds, min <= max, effective > 0");
      continue;
    }
    m_steps.push_back(step);
  }

  for (size_t i = 0; i < m_steps.size(); ++i) {
    for (size_t j = 0; j < i; ++j) {
      const ExposureStep &earlier = m_steps[j];
      const ExposureStep &step = m_steps[i];
      const bool separated =
          step.requested_max_us < earlier.requested_min_us || step.requested_min_us > earlier.requested_max_us;
      if (!separated) {
        ScanLogError("Exposure step table has overlapping request ranges; the earlier row wins");
        break;
      }
    }
  }
}

const ExposureStepTable &ExposureStepTable::Placeholder() {
  static const ExposureStepTable table(PlaceholderRows(), kExposureStepTablePlaceholderNote, true);
  return table;
}

const ExposureStepTable &ActiveExposureStepTable() { return ExposureStepTable::Placeholder(); }

ExposureMatch ExposureStepTable::Match(double requested_us, bool log) const {
  ExposureMatch match;
  match.requested_us = requested_us;
  match.effective_us = requested_us;
  match.step_index = -1;

  if (!std::isfinite(requested_us)) {
    if (log) {
      ScanLogError("Exposure: requested value is not finite; effective left equal to the request");
    }
    return match;
  }

  const ExposureStep *step = nullptr;
  for (int i = 0; i < static_cast<int>(m_steps.size()); ++i) {
    const ExposureStep &row = m_steps[static_cast<size_t>(i)];
    if (requested_us >= row.requested_min_us && requested_us <= row.requested_max_us) {
      match.effective_us = row.effective_us;
      match.step_index = i;
      step = &row;
      break;
    }
  }

  if (!log) {
    return match;
  }

  if (step != nullptr) {
    const char *kind = step->origin == ExposureStepOrigin::ObservedExample ? "Max example" : "placeholder row";
    ScanLogMessageF("Exposure: requested %.6g us -> effective %.6g us, step %d (%s). %s", requested_us,
                    match.effective_us, match.step_index, kind, m_note.c_str());
  } else {
    ScanLogMessageF(
        "Exposure: requested %.6g us is outside the step table; effective left equal to the request. %s",
        requested_us, m_note.c_str());
  }
  return match;
}

double ExposureStepTable::EffectiveUs(double requested_us, bool log) const {
  return Match(requested_us, log).effective_us;
}

std::string FormatExposureSessionComment(const SessionExposureMetadata &metadata) {
  std::string requested;
  std::string effective;
  for (size_t i = 0; i < metadata.exposures.size(); ++i) {
    if (i != 0) {
      requested += ',';
      effective += ',';
    }
    requested += FormatUs(metadata.exposures[i].requested_us);
    effective += FormatUs(metadata.exposures[i].effective_us);
  }

  std::string text = metadata.table_note.empty() ? std::string(kExposureStepTablePlaceholderNote) : metadata.table_note;
  text += " requested_us=";
  text += requested;
  text += " effective_us=";
  text += effective;
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

  out << "{\n";
  out << "  \"exposure_step_table\": \"" << (metadata.table_placeholder ? "placeholder" : "custom") << "\",\n";
  const std::string note =
      metadata.table_note.empty() ? std::string(kExposureStepTablePlaceholderNote) : metadata.table_note;
  out << "  \"exposure_step_table_note\": \"" << JsonEscape(note) << "\",\n";
  out << "  \"exposures\": [\n";
  for (size_t i = 0; i < metadata.exposures.size(); ++i) {
    const ExposureObservation &row = metadata.exposures[i];
    out << "    {\"requested_us\": " << FormatUs(row.requested_us) << ", \"effective_us\": " << FormatUs(row.effective_us)
        << ", \"step_index\": " << row.step_index << "}";
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

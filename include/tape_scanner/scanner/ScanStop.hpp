#pragma once

// Останов и итог скана (ACQ-05, CTL-05, T21).
// Без wxWidgets, SDK камеры и сокетов: и рабочий Scanner, и симулятор
// вызывают один и тот же PlanTerminal.
//
// Допущения, пока нет паспорта механики и ответа по обрыву:
// - hold_motors = 1 и save_captured = 1, как в секции [link];
// - эти два флага относятся к обрыву. Останов оператора и авария устройства
//   всегда оставляют оси на месте, без home и без выдачи;
// - выдача (+40000) и парковка не входят в хвост цикла. Это отдельные
//   команды, и только после статуса Completed;
// - 2 с — срок подтверждения Stop в программе, не путь торможения;
// - ожидание плёнки ограничено 30 мин и прерывается отменой;
// - повторных попыток после обрыва нет.

#include <chrono>
#include <cctype>
#include <string>

namespace tape_scanner {

inline constexpr int kFilmEjectSteps = 40000;

inline constexpr std::chrono::milliseconds kDefaultStopDeadline{2000};
inline constexpr std::chrono::milliseconds kDefaultMotionDeadline{2000};
inline constexpr std::chrono::milliseconds kDefaultCaptureDeadline{2000};
inline constexpr std::chrono::milliseconds kDefaultWriteDeadline{2000};
inline constexpr std::chrono::milliseconds kDefaultFilmWait{30 * 60 * 1000};

enum class ScanRunStatus { Idle, Running, Completed, Partial, Fault };

enum class ScanStopReason {
  None,
  OperatorStop,
  EndOfMedia,
  EndOfCalibration,
  Finished,
  MissingCalibration,
  EmptyFrame,
  HdrFailed,
  PushFailed,
  FinishFailed,
  LinkLost,
  DeviceFault,
  Exception,
  NotCreated,
  StopTimeout,
  PreflightFailed,
  FilmTimeout,
  MotionTimeout,
  CaptureTimeout,
  WriteTimeout
};

enum class ScanPhase {
  Idle,
  Connecting,
  WaitingForFilm,
  Referencing,
  Moving,
  WaitingFrame,
  Writing,
  Stopping,
  Finished
};

struct ScanStopSettings {
  bool hold_motors = true;
  bool save_captured = true;
  std::chrono::milliseconds stop_deadline = kDefaultStopDeadline;
  std::chrono::milliseconds motion_deadline = kDefaultMotionDeadline;
  std::chrono::milliseconds capture_deadline = kDefaultCaptureDeadline;
  std::chrono::milliseconds write_deadline = kDefaultWriteDeadline;
  std::chrono::milliseconds film_wait = kDefaultFilmWait;
};

struct TerminalInput {
  ScanStopReason reason = ScanStopReason::None;
  std::string message;
  bool hold_motors = true;
  bool save_captured = true;
  bool controller_link_up = true;
  bool axes_live = false;
  bool completeness_proven = false;
  bool finish_ok = true;
  int confirmed_tiles = 0;
  int open_tiles = 0;
};

struct TerminalPlan {
  ScanRunStatus status = ScanRunStatus::Idle;
  bool stop_axes = false;
  bool home_axes = false;
  bool eject_film = false;
  bool light_off = false;
  bool save_open = false;
  bool claim_success = false;
};

struct ScanRunResult {
  ScanRunStatus status = ScanRunStatus::Idle;
  ScanStopReason reason = ScanStopReason::None;
  std::string message;
  int confirmed_tiles = 0;
  int open_tiles = 0;
  bool claim_success = false;
  bool timed_out = false;
  ScanPhase phase = ScanPhase::Idle;
};

struct ScanStopResult {
  bool accepted = false;
  bool timed_out = false;
  ScanRunStatus status = ScanRunStatus::Idle;
  ScanStopReason reason = ScanStopReason::NotCreated;
  ScanRunResult run;
};

inline const char *ScanStopReasonText(ScanStopReason reason) {
  switch (reason) {
  case ScanStopReason::None:
    return "none";
  case ScanStopReason::OperatorStop:
    return "operator stop";
  case ScanStopReason::EndOfMedia:
    return "end of media";
  case ScanStopReason::EndOfCalibration:
    return "calibration finished";
  case ScanStopReason::Finished:
    return "expected tiles captured";
  case ScanStopReason::MissingCalibration:
    return "missing calibration";
  case ScanStopReason::EmptyFrame:
    return "empty frame";
  case ScanStopReason::HdrFailed:
    return "hdr failed";
  case ScanStopReason::PushFailed:
    return "tile rejected";
  case ScanStopReason::FinishFailed:
    return "finish failed";
  case ScanStopReason::LinkLost:
    return "link lost";
  case ScanStopReason::DeviceFault:
    return "device fault";
  case ScanStopReason::Exception:
    return "exception";
  case ScanStopReason::NotCreated:
    return "scanner is not created";
  case ScanStopReason::StopTimeout:
    return "stop timed out";
  case ScanStopReason::PreflightFailed:
    return "preflight failed";
  case ScanStopReason::FilmTimeout:
    return "film wait timed out";
  case ScanStopReason::MotionTimeout:
    return "motion timed out";
  case ScanStopReason::CaptureTimeout:
    return "capture timed out";
  case ScanStopReason::WriteTimeout:
    return "write timed out";
  }
  return "unknown";
}

inline const char *ScanRunStatusText(ScanRunStatus status) {
  switch (status) {
  case ScanRunStatus::Idle:
    return "idle";
  case ScanRunStatus::Running:
    return "running";
  case ScanRunStatus::Completed:
    return "completed";
  case ScanRunStatus::Partial:
    return "partial";
  case ScanRunStatus::Fault:
    return "fault";
  }
  return "unknown";
}

inline std::string TrimmedLower(std::string text) {
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

// Пустая строка и мусор читаются как fallback (для этих ключей — истина),
// чтобы опечатка не уводила моторы в ноль и не выбрасывала снимок.
inline bool ParseStopBool(const std::string &text, bool fallback) {
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

inline ScanStopSettings ScanStopSettingsFromText(const std::string &hold_motors,
                                                 const std::string &save_captured) {
  ScanStopSettings settings;
  settings.hold_motors = ParseStopBool(hold_motors, true);
  settings.save_captured = ParseStopBool(save_captured, true);
  return settings;
}

inline bool IsNormalEnd(ScanStopReason reason) {
  return reason == ScanStopReason::Finished || reason == ScanStopReason::EndOfMedia ||
         reason == ScanStopReason::EndOfCalibration;
}

inline bool KeepsCaptured(const TerminalInput &in, bool save_open) {
  return in.confirmed_tiles > 0 || (save_open && in.open_tiles > 0);
}

inline TerminalPlan PlanTerminal(const TerminalInput &in) {
  TerminalPlan plan;
  plan.eject_film = false;
  plan.save_open = in.save_captured && in.reason != ScanStopReason::None &&
                   in.reason != ScanStopReason::NotCreated;
  plan.light_off = in.reason != ScanStopReason::None && in.reason != ScanStopReason::NotCreated;

  const bool kept = KeepsCaptured(in, plan.save_open);

  switch (in.reason) {
  case ScanStopReason::None:
  case ScanStopReason::NotCreated:
    plan.status = ScanRunStatus::Idle;
    plan.light_off = false;
    plan.save_open = false;
    break;
  case ScanStopReason::OperatorStop:
  case ScanStopReason::StopTimeout:
    // Останов оператора не смотрит на hold_motors: оси стопорятся, home нет.
    plan.stop_axes = true;
    plan.status = ScanRunStatus::Partial;
    break;
  case ScanStopReason::EndOfMedia:
    plan.status = (in.completeness_proven && in.finish_ok && in.confirmed_tiles > 0)
                      ? ScanRunStatus::Completed
                      : ScanRunStatus::Partial;
    plan.claim_success = false;
    break;
  case ScanStopReason::EndOfCalibration:
    plan.status = ScanRunStatus::Completed;
    plan.claim_success = false;
    break;
  case ScanStopReason::Finished:
    if (in.completeness_proven && in.finish_ok && in.confirmed_tiles > 0) {
      plan.status = ScanRunStatus::Completed;
      plan.claim_success = true;
    } else {
      plan.status = ScanRunStatus::Partial;
    }
    break;
  case ScanStopReason::MissingCalibration:
  case ScanStopReason::PreflightFailed:
    plan.stop_axes = in.axes_live;
    plan.status = ScanRunStatus::Fault;
    break;
  case ScanStopReason::LinkLost:
    if (in.controller_link_up && in.axes_live) {
      if (in.hold_motors) {
        plan.stop_axes = true;
      } else {
        plan.home_axes = true;
      }
    }
    plan.status = kept ? ScanRunStatus::Partial : ScanRunStatus::Fault;
    break;
  case ScanStopReason::EmptyFrame:
  case ScanStopReason::HdrFailed:
  case ScanStopReason::PushFailed:
  case ScanStopReason::FinishFailed:
  case ScanStopReason::DeviceFault:
  case ScanStopReason::Exception:
  case ScanStopReason::FilmTimeout:
  case ScanStopReason::MotionTimeout:
  case ScanStopReason::CaptureTimeout:
  case ScanStopReason::WriteTimeout:
    plan.stop_axes = in.axes_live;
    plan.status = kept ? ScanRunStatus::Partial : ScanRunStatus::Fault;
    break;
  }

  if (IsNormalEnd(in.reason)) {
    plan.stop_axes = false;
    plan.home_axes = false;
  }
  plan.eject_film = false;
  if (plan.status != ScanRunStatus::Completed || in.reason != ScanStopReason::Finished) {
    plan.claim_success = false;
  }
  return plan;
}

// Отказ записи нельзя показать как успешное завершение.
inline void ApplyFinishResult(TerminalPlan &plan, ScanStopReason &reason, bool finish_ok,
                              int committed_after) {
  if (finish_ok) {
    if (plan.status == ScanRunStatus::Completed && committed_after <= 0) {
      plan.status = ScanRunStatus::Partial;
      plan.claim_success = false;
    }
    return;
  }
  plan.claim_success = false;
  if (plan.status == ScanRunStatus::Completed || reason == ScanStopReason::Finished ||
      reason == ScanStopReason::EndOfMedia || reason == ScanStopReason::EndOfCalibration) {
    reason = ScanStopReason::FinishFailed;
  }
  if (reason == ScanStopReason::OperatorStop) {
    plan.status = ScanRunStatus::Partial;
    return;
  }
  plan.status = committed_after > 0 ? ScanRunStatus::Partial : ScanRunStatus::Fault;
}

// Stop до создания объекта не разыменовывает указатель и не ждёт поток.
template <typename Scanner>
ScanStopResult RequestScanStop(Scanner *scanner) {
  if (scanner == nullptr) {
    ScanStopResult result;
    result.accepted = false;
    result.reason = ScanStopReason::NotCreated;
    result.status = ScanRunStatus::Idle;
    return result;
  }
  return scanner->RequestStop();
}

}  // namespace tape_scanner

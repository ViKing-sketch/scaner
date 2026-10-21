#pragma once

#include <stdexcept>
#include <string>

class ScanSession;

// Обрыв связи. Ключи settings.ini, секция [link]. Окна настроек нет:
//   hold_motors = 1    моторы остаются на месте (стоп, без home и без нуля)
//   hold_motors = 0    после обрыва увести моторы в ноль, когда контроллер ответит
//   save_captured = 1  записать уже снятые тайлы
//   save_captured = 0  открытый холст не писать; уже закрытые файлы не удалять
// Непонятное значение читается как 1, чтобы опечатка не уводила моторы в ноль
// и не выбрасывала снимок.

inline constexpr const char *kLinkSection = "link";
inline constexpr const char *kHoldMotorsKey = "hold_motors";
inline constexpr const char *kSaveCapturedKey = "save_captured";

enum class LinkEnd { None, Camera, Controller };

enum class LinkPhase { Unknown, Up, Down, Reconnecting };

enum class MotorLossAction { Hold, Home };

class LinkLost : public std::runtime_error {
public:
  explicit LinkLost(LinkEnd which)
      : std::runtime_error(which == LinkEnd::Camera ? "Camera link lost" : "Controller link lost"),
        end(which) {}

  LinkEnd end;
};

struct LinkLossSettings {
  bool hold_motors = true;
  bool save_captured = true;
};

struct LinkEdge {
  LinkPhase next = LinkPhase::Unknown;
  bool became_lost = false;
  bool became_up = false;
};

struct LinkLossNote {
  bool visible = false;
  bool scan_stopped = false;
  bool mention_capture = false;
  MotorLossAction motors = MotorLossAction::Hold;
  bool save_captured = true;
  bool save_ok = true;
  int saved_segments = 0;
};

struct LinkLossResult {
  MotorLossAction motors = MotorLossAction::Hold;
  bool save_captured = true;
  bool save_ok = true;
  int saved_segments = 0;
  bool discarded = false;
};

struct ScanLinkLoss {
  LinkEnd end = LinkEnd::None;
  bool motors_commanded = false;
  MotorLossAction motors = MotorLossAction::Hold;
  bool save_captured = true;
  bool save_ok = true;
  int saved_segments = 0;
  bool mention_capture = true;
};

bool ParseIniBool(const std::string &text, bool fallback);

LinkLossSettings LinkLossSettingsFromText(const std::string &hold_motors, const std::string &save_captured);

/// Повторные пробы, пока связь не вернулась, новый обрыв не создают.
LinkEdge AdvanceLink(LinkPhase previous, bool up);

const char *CameraLinkPhrase(LinkPhase phase);
const char *ControllerLinkPhrase(LinkPhase phase);

std::string FormatLinkStatus(LinkPhase camera, LinkPhase controller, const LinkLossNote &note);

/// save_captured пишет хвост через Finish(). Иначе Discard(): уже закрытые сегменты остаются.
LinkLossResult ApplyLinkLoss(ScanSession *session, const LinkLossSettings &settings);

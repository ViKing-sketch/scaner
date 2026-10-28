#pragma once

// Прогон позиционного скана на ICamera/IController.
// TCP контроллера здесь нет: движение — логические Move/Home/StopScan.
// Симулятор применяет команду сразу; пауза фазы изображает ожидание.

#include "tape_scanner/adapters/icamera.hpp"
#include "tape_scanner/adapters/icontroller.hpp"
#include "tape_scanner/scanner/ScanStop.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tape_scanner {

struct CapturedTile {
  std::uint64_t tile_id = 0;
  int width = 0;
  int height = 0;
  std::size_t bytes = 0;
  double carriage = 0;
  double film = 0;
};

class TileSink {
 public:
  virtual ~TileSink() = default;
  virtual bool Push(const CapturedTile &tile) = 0;
  virtual bool Commit() = 0;
  virtual bool Finish() = 0;
  virtual void DiscardOpen() = 0;
  [[nodiscard]] virtual int Committed() const = 0;
  [[nodiscard]] virtual int Open() const = 0;
};

enum class MotionKind {
  ReferenceHome,
  MoveCarriage,
  MoveFilm,
  StopAxes,
  Eject,
  Park,
  LightOn,
  LightOff
};

struct MotionRecord {
  MotionKind kind = MotionKind::StopAxes;
  double position = 0;
  ScanPhase phase = ScanPhase::Idle;
};

struct ScanJob {
  std::string scanner_id = "scn-a";
  std::string session_id = "ses-a";
  std::string job_id = "job-a";
  std::string camera_serial = "CAM-A";
  std::string controller_serial = "CTL-A";
  int expected_tiles = 1;
  int exposures_per_tile = 1;
  double carriage_origin = 10;
  double carriage_step = 1;
  double film_origin = 0;
  double film_step = 0;
  bool require_calibration = true;
  bool calibration_present = true;
};

struct ScanHooks {
  // cancel становится истинным, когда RequestStop уже вызван.
  // Хук, который ждёт железо, обязан вернуться после cancel, иначе
  // подтверждение Stop упрётся в stop_deadline и не будет ждать поток дальше.
  std::function<void(ScanPhase phase, const std::atomic<bool> &cancel)> on_phase;
  // Пустой хук — датчик из GetState. Иначе это сигнал конца носителя в тесте:
  // у симулятора Z02 датчик плёнки не меняется после создания.
  std::function<bool()> film_present;
  // Пустой хук — ICamera::Capture. Тест подменяет пустой кадр, HDR и исключение.
  std::function<adapters::AdapterResult(const adapters::CaptureRequest &, adapters::Frame &)> capture;
};

struct DirectorResult : ScanRunResult {
  std::vector<MotionRecord> motions;
};

class ScanDirector {
 public:
  ScanDirector(adapters::ICamera &camera, adapters::IController &controller,
               ScanStopSettings settings = {});
  ~ScanDirector();

  ScanDirector(const ScanDirector &) = delete;
  ScanDirector &operator=(const ScanDirector &) = delete;

  void SetSettings(ScanStopSettings settings);

  // Синхронный прогон на вызывающем потоке.
  DirectorResult Run(const ScanJob &job, TileSink &sink, ScanHooks hooks = {});

  // Фоновый прогон. sink живёт до join, то есть до RequestStop или деструктора.
  void Start(const ScanJob &job, TileSink &sink, ScanHooks hooks = {});

  ScanStopResult RequestStop();
  [[nodiscard]] ScanPhase Phase() const;
  [[nodiscard]] DirectorResult Result() const;

  // Отдельные команды. Отклоняются, пока идёт прогон и если итог не Completed.
  adapters::AdapterResult EjectFilm();
  adapters::AdapterResult Park();

 private:
  void ThreadMain();
  void Execute();
  void RunJob();
  void ApplyTerminal();
  bool Enter(ScanPhase phase);
  bool FilmPresent();
  void RememberCounts();
  void Record(MotionKind kind, double position);
  void Light(bool on);
  adapters::AdapterResult CaptureOne(std::uint64_t tile_id, std::uint64_t exposure_id,
                                     adapters::Frame &frame);
  void ClassifyCapture(adapters::AdapterError code, const std::string &message);

  adapters::ICamera &m_camera;
  adapters::IController &m_controller;
  ScanStopSettings m_settings;
  ScanJob m_job;
  TileSink *m_sink = nullptr;
  ScanHooks m_hooks;
  TerminalInput m_input;
  DirectorResult m_result;
  mutable std::mutex m_mu;
  std::condition_variable m_cv;
  std::thread m_thread;
  std::thread::id m_worker_id;
  bool m_running = false;
  bool m_done = false;
  int m_command_serial = 0;
  std::atomic<bool> m_cancel{false};
  std::atomic<ScanPhase> m_phase{ScanPhase::Idle};
};

}  // namespace tape_scanner

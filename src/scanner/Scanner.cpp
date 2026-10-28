#include <tape_scanner/scanner/Scanner.hpp>
#include <tape_scanner/scanner/ScanSession.hpp>
#include <tape_scanner/scanner/Utils.hpp>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <wx/log.h>
#include <wx/strvararg.h>
#include <wx/thread.h>

namespace {
constexpr std::string COEFS_FILE_NAME = R"(./coefs.xml)";
constexpr double EXPOSE_DESCRET = 16.0;
}

Scanner::Scanner(Controller& controller, Camera& camera)
    : m_controller(controller), m_camera(camera) {
    InitScannerCalibrate();
}

Scanner::~Scanner() {
  m_operator_stop.store(true);
  m_running.store(false);
  if (m_thread && m_thread->joinable()) {
    m_thread->join();
  }
}

void Scanner::StartScan(const std::string& dst_scan_file_path, bool async) {
  wxLogMessage("StartScan");

  if (m_running) {
    wxLogWarning("Scanning already started!");
    return;
  }
  if (m_thread && m_thread->joinable()) {
    wxLogWarning("Previous scan is still stopping");
    return;
  }

    m_dst_scan_file_path = dst_scan_file_path;
    m_session = std::make_unique<ScanSession>(m_dst_scan_file_path, m_geometry);
    m_operator_stop.store(false);
    m_reason = tape_scanner::ScanStopReason::None;
    m_fail_message.clear();
    m_axes_live = false;
    m_film_wait_timed_out = false;
    m_terminal_done.store(false);
    m_result = {};
    m_cur_dir = -1;
    {
      std::lock_guard lock(m_wait_mu);
      m_worker_active = true;
      m_thread_finished = false;
    }
    m_running.store(true);

    if (async) {
        m_thread = std::make_unique<std::thread>(&Scanner::WorkerLoop, this);
    } else {
        WorkerLoop();
    }
}

void Scanner::StopScan() {
  wxLogMessage("StopScan requested");
  const tape_scanner::ScanStopResult stop = RequestStop();
  if (stop.timed_out) {
    wxLogWarning("Stop is still waiting for the scan thread");
    return;
  }
  wxLogMessage("Scan stopped");
}

tape_scanner::ScanStopResult Scanner::RequestStop() {
  m_operator_stop.store(true);
  m_running.store(false);

  tape_scanner::ScanStopResult result;
  result.accepted = true;
  if (m_worker_id == std::this_thread::get_id()) {
    result.reason = tape_scanner::ScanStopReason::OperatorStop;
    result.status = tape_scanner::ScanRunStatus::Partial;
    return result;
  }

  std::unique_lock lock(m_wait_mu);
  if (!m_worker_active) {
    result.reason = m_result.reason == tape_scanner::ScanStopReason::None
                        ? tape_scanner::ScanStopReason::OperatorStop
                        : m_result.reason;
    result.status = m_result.status;
    result.run = m_result;
    return result;
  }

  const bool finished = m_worker_cv.wait_for(lock, m_stop_settings.stop_deadline, [this] {
    return m_thread_finished;
  });
  if (!finished) {
    result.timed_out = true;
    result.reason = tape_scanner::ScanStopReason::StopTimeout;
    result.status = tape_scanner::ScanRunStatus::Fault;
    return result;
  }
  lock.unlock();
  if (m_thread && m_thread->joinable()) {
    m_thread->join();
    m_thread.reset();
  }
  lock.lock();
  result.reason = m_result.reason;
  result.status = m_result.status;
  result.run = m_result;
  return result;
}

tape_scanner::ScanRunResult Scanner::Result() const {
  std::lock_guard lock(m_wait_mu);
  return m_result;
}

bool Scanner::IsRunning() {
    return m_running;
}

void Scanner::AutoCalibrateCamera() {
    GoCameraToStartPosition();
    int dummy = 50;
    while (dummy--) {
        m_camera.GetMat();
    }

    auto first_cadr = m_camera.GetMat();
    cv::imshow("first_cadr", first_cadr);
    cv::Point first_point = {m_cur_camera_pos, m_cur_roller_pos};

    m_cur_camera_pos += 1000;
    m_controller.GoCameraTo(m_cur_camera_pos);
    m_cur_roller_pos += 1000;
    m_controller.GoRollersTo(m_cur_roller_pos);

    dummy = 50;
    while (dummy--) {
        m_camera.GetMat();
    }
    auto second_cadr = m_camera.GetMat();
    cv::imshow("second_cadr", second_cadr);
    cv::Point second_point = {m_cur_camera_pos, m_cur_roller_pos};

    auto pixel_shift = calcShiftBetweenCadres(first_cadr, second_cadr);
    auto step_shift = second_point - first_point;

    m_coefs.x_steps_in_pixsel = std::abs(step_shift.x / pixel_shift.x);
    m_coefs.y_steps_in_pixsel = std::abs(step_shift.y / pixel_shift.y);

    wxLogMessage(wxString::Format("Auto calibration complete: x: %g, y: %g",
                                  m_coefs.x_steps_in_pixsel,
                                  m_coefs.y_steps_in_pixsel));

    cv::waitKey();
}

void Scanner::CalibrateCamera() {
    m_camera.SetExpose(10);
    GoCameraToStartPosition();
    auto cadr = m_camera.GetMat();
    cv::Mat sum(cadr.size(), CV_64FC1);

    int counter = 0;
    while (m_cur_camera_pos > m_coefs.camera_min_pos) {
        cadr = m_camera.GetMat();
        sum += cadr;

        m_cur_camera_pos -= 500;
        m_controller.GoCameraTo(m_cur_camera_pos);

        ++counter;
    }

    auto average = cv::mean(sum);
    auto calibr = average / sum;

    cv::FileStorage fs(R"(./calibr.xml)", cv::FileStorage::WRITE);
    fs << "mat" << calibr;
    fs.release();

    cv::imshow("calibr", calibr);
}

void Scanner::CalibrateScanner() {
  m_calibrates = {.cadres_by_pos = {},
                  .sum_aver = 0,
                  .total_cadres = 0,
                  .coefs_by_pos = {},
                  .iter_cnt = 0};
  m_calibrate_running = true;

  // m_controller.LightEnable(false);
  GoCameraToCentre();

  m_calibrates.iter_cnt = 6 * m_coefs.roller_step_size;

  StartScan("./film.dcm", false);
  CalculateCalibrates();
  m_calibrate_running = false;

  wxLogMessage("Scanner: Calibrate finished");
}

void Scanner::WorkerLoop() {
  m_worker_id = std::this_thread::get_id();
  try {
    RunWorker();
  } catch (const std::exception &ex) {
    FinishTerminal(MakeInput(tape_scanner::ScanStopReason::Exception, ex.what()));
  } catch (...) {
    FinishTerminal(MakeInput(tape_scanner::ScanStopReason::Exception, "unknown exception"));
  }
  {
    std::lock_guard lock(m_wait_mu);
    m_thread_finished = true;
    m_worker_active = false;
    m_running.store(false);
  }
  m_worker_cv.notify_all();
}

void Scanner::RunWorker() {
  if (IsCanceled()) {
    FinishTerminal(MakeInput(tape_scanner::ScanStopReason::OperatorStop, "stop before motion"));
    return;
  }

  // Старт по-прежнему ищет ноль каретки. Это не хвост после отмены.
  m_axes_live = true;
  m_controller.GoCameraToHome(true);
  if (IsCanceled()) {
    FinishTerminal(MakeInput(tape_scanner::ScanStopReason::OperatorStop, "stop during reference"));
    return;
  }

  if (!WaitFilm()) {
    const auto reason = m_film_wait_timed_out ? tape_scanner::ScanStopReason::FilmTimeout
                                              : tape_scanner::ScanStopReason::OperatorStop;
    FinishTerminal(MakeInput(reason, m_film_wait_timed_out ? "film wait timed out"
                                                          : "stop while waiting for film"));
    return;
  }
  if (IsCanceled()) {
    FinishTerminal(MakeInput(tape_scanner::ScanStopReason::OperatorStop, "stop"));
    return;
  }

  if (!m_light_only_when_capture) {
    m_controller.LightEnable(true);
  }

  ResetRollersPosition();
  m_controller.GoRollersTo(16000, true);
  if (m_reason != tape_scanner::ScanStopReason::None) {
    FinishTerminal(MakeInput(m_reason, m_fail_message));
    return;
  }
  if (IsCanceled()) {
    FinishTerminal(MakeInput(tape_scanner::ScanStopReason::OperatorStop, "stop during approach"));
    return;
  }

  GoCameraToStartPosition();

  while (!IsCanceled() && m_reason == tape_scanner::ScanStopReason::None) {
    CaptureCadr();
    if (IsCanceled() || m_reason != tape_scanner::ScanStopReason::None) {
      break;
    }
    GoCameraToNextPosition();
  }

  auto reason = m_reason;
  if (reason == tape_scanner::ScanStopReason::None) {
    reason = tape_scanner::ScanStopReason::OperatorStop;
  }
  FinishTerminal(MakeInput(reason, m_fail_message));
}

tape_scanner::TerminalInput Scanner::MakeInput(tape_scanner::ScanStopReason reason,
                                              std::string message) const {
  tape_scanner::TerminalInput input;
  input.reason = reason;
  input.message = std::move(message);
  input.hold_motors = m_stop_settings.hold_motors;
  input.save_captured = m_stop_settings.save_captured;
  input.controller_link_up = true;
  input.axes_live = m_axes_live;
  input.completeness_proven = false;
  input.finish_ok = true;
  if (m_session) {
    input.confirmed_tiles = m_session->SavedSegmentCount();
    input.open_tiles = m_session->HasOpenPixels() ? 1 : 0;
  }
  return input;
}

void Scanner::FinishTerminal(tape_scanner::TerminalInput input) {
  if (m_terminal_done.exchange(true)) {
    return;
  }

  auto plan = tape_scanner::PlanTerminal(input);
  try {
    if (plan.stop_axes) {
      m_controller.HaltAxes();
    }
  } catch (const std::exception &ex) {
    wxLogError(wxString::Format("Halt failed: %s", wxString(ex.what())));
  }
  try {
    if (plan.home_axes) {
      m_controller.GoCameraToHome(true);
    }
  } catch (const std::exception &ex) {
    wxLogError(wxString::Format("Home after link loss failed: %s", wxString(ex.what())));
  }
  try {
    // Выдача не выполняется сама: PlanTerminal не ставит eject_film
    // ни для отмены, ни для аварии, ни для конца носителя.
    if (plan.eject_film) {
      m_cur_roller_pos += tape_scanner::kFilmEjectSteps;
      m_controller.GoRollersTo(m_cur_roller_pos, true);
    }
  } catch (const std::exception &ex) {
    wxLogError(wxString::Format("Eject failed: %s", wxString(ex.what())));
  }
  try {
    if (plan.light_off) {
      m_controller.LightEnable(false);
    }
  } catch (const std::exception &ex) {
    wxLogError(wxString::Format("Light off failed: %s", wxString(ex.what())));
  }

  bool finish_ok = true;
  int committed = input.confirmed_tiles;
  if (m_session) {
    try {
      if (plan.save_open) {
        finish_ok = m_session->Finish();
      } else {
        m_session->Discard();
      }
      committed = m_session->SavedSegmentCount();
    } catch (const std::exception &ex) {
      finish_ok = false;
      wxLogError(wxString::Format("Scan save failed: %s", wxString(ex.what())));
    }
  }
  tape_scanner::ApplyFinishResult(plan, input.reason, finish_ok, committed);

  tape_scanner::ScanRunResult result;
  result.status = plan.status;
  result.reason = input.reason;
  result.message = input.message;
  result.confirmed_tiles = committed;
  result.open_tiles = (m_session && m_session->HasOpenPixels()) ? 1 : 0;
  result.claim_success = plan.claim_success;
  {
    std::lock_guard lock(m_wait_mu);
    m_result = result;
  }

  const wxString reason_text(tape_scanner::ScanStopReasonText(input.reason));
  if (plan.claim_success) {
    wxLogMessage(wxString::Format("Scan completed, segments: %d", committed));
  } else if (plan.status == tape_scanner::ScanRunStatus::Partial) {
    wxLogMessage(wxString::Format("Scan partial (%s), saved segments: %d", reason_text, committed));
  } else if (plan.status == tape_scanner::ScanRunStatus::Fault) {
    wxLogError(wxString::Format("Scan fault (%s), saved segments: %d", reason_text, committed));
  } else {
    wxLogMessage(wxString::Format("Scan ended (%s)", reason_text));
  }
}

void Scanner::NoteFailure(tape_scanner::ScanStopReason reason, const std::string &message) {
  if (m_reason == tape_scanner::ScanStopReason::None) {
    m_reason = reason;
    m_fail_message = message;
  }
  m_running.store(false);
  wxLogError(wxString::Format("Scan: %s", wxString(message.c_str())));
}

bool Scanner::WaitFilm() {
  const auto deadline = std::chrono::steady_clock::now() + m_stop_settings.film_wait;
  while (!HasFilm()) {
    if (IsCanceled()) {
      return false;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      m_film_wait_timed_out = true;
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return true;
}

bool Scanner::IsCanceled() {
  return m_operator_stop.load() || !m_running.load();
}

bool Scanner::HasFilm() {
    return m_controller.HasFilm() || m_calibrate_running;
}

bool Scanner::HasFilmInCamera() {
    throw std::logic_error("Not implemented!");
}

void Scanner::GoCameraToStartPosition() {
    if (IsCanceled() || m_reason != tape_scanner::ScanStopReason::None) {
      return;
    }
    m_cur_camera_pos = m_coefs.camera_max_pos;
    m_axes_live = true;
    m_controller.GoCameraTo(m_cur_camera_pos, true);

    ResetRollersPosition();
}

void Scanner::ResetRollersPosition()
{
  m_cur_roller_pos = 0;
  m_controller.ResetRollerPos();
}

void Scanner::GoCameraToCentre() {
    m_cur_camera_pos = (m_coefs.camera_max_pos - m_coefs.camera_min_pos) / 2 +
                       m_coefs.camera_min_pos;
    m_controller.GoCameraTo(m_cur_camera_pos, true);
}

void Scanner::GoFilmToNextRow() {
    if (IsCanceled()) {
      if (m_reason == tape_scanner::ScanStopReason::None) {
        m_reason = tape_scanner::ScanStopReason::OperatorStop;
      }
      return;
    }
    if (m_calibrate_running && m_cur_roller_pos >= m_calibrates.iter_cnt) {
        m_reason = tape_scanner::ScanStopReason::EndOfCalibration;
        return;
    }

    if (!HasFilm()) {
        m_reason = tape_scanner::ScanStopReason::EndOfMedia;
        return;
    }

    auto old_pos = m_cur_roller_pos;
    m_cur_roller_pos += m_coefs.roller_step_size;

    if (old_pos != m_cur_roller_pos) {
        m_controller.GoRollersTo(m_cur_roller_pos, true);
    }
}

void Scanner::GoCameraToNextPosition() {
    if (IsCanceled() || m_reason != tape_scanner::ScanStopReason::None) {
      return;
    }
    int old_pos = m_cur_camera_pos;
    m_cur_camera_pos += m_cur_dir * m_coefs.camera_step_size;

    if (m_cur_camera_pos > m_coefs.camera_max_pos ||
        m_cur_camera_pos < m_coefs.camera_min_pos) {
        m_cur_dir *= -1;
        m_cur_camera_pos = std::min(m_cur_camera_pos, m_coefs.camera_max_pos);
        m_cur_camera_pos = std::max(m_cur_camera_pos, m_coefs.camera_min_pos);

        m_axes_live = true;
        m_controller.GoCameraTo(m_cur_camera_pos, true);
        if (IsCanceled() || m_reason != tape_scanner::ScanStopReason::None) {
          return;
        }
        CaptureCadr();
        if (IsCanceled() || m_reason != tape_scanner::ScanStopReason::None) {
          return;
        }

        GoFilmToNextRow();
        if (m_reason != tape_scanner::ScanStopReason::None || IsCanceled()) {
          return;
        }
        if (HasFilm())
            CaptureCadr();
        if (m_reason != tape_scanner::ScanStopReason::None || IsCanceled()) {
          return;
        }

        m_cur_camera_pos += m_cur_dir * m_coefs.camera_step_size;
    }
    if (old_pos != m_cur_camera_pos) {
        m_axes_live = true;
        m_controller.GoCameraTo(m_cur_camera_pos, true);
    }
}

void Scanner::CaptureCadr(bool need_save) {
  (void)need_save;
  if (IsCanceled() || m_reason != tape_scanner::ScanStopReason::None) {
    return;
  }
  wxLogMessage("CaptureCadr start");

  cv::Mat cadr; // result
  try {
  if (!m_calibrate_running &&
      m_calibrates.coefs_by_pos.contains(m_cur_camera_pos)) {

    if(m_light_only_when_capture)
      m_controller.LightEnable(true);

    cv::Mat cadr_with_exp = m_camera.GetCadrWithExposes({16, 96, 320, 1600, 4400, 8000, 14000, 20000, 28000, 30720});

    if(m_light_only_when_capture)
      m_controller.LightEnable(false);

    auto flat = m_calibrates.coefs_by_pos[m_cur_camera_pos];

    cv::Mat calibr_cadr = cadr_with_exp.mul(flat);

    calibr_cadr.convertTo(cadr, CV_16UC1, 65535.0);
  } else if (!m_calibrate_running) {
    NoteFailure(tape_scanner::ScanStopReason::MissingCalibration,
                "calibration is missing for the camera position");
    return;
  }

  if (!m_calibrate_running) {
    if (cadr.empty() || cadr.type() != CV_16UC1) {
      NoteFailure(tape_scanner::ScanStopReason::EmptyFrame, "empty or unexpected frame");
      return;
    }
    auto &b = m_coefs.border;
    cv::Rect after_croped = {b.x, b.y, cadr.cols - b.x - b.width, cadr.rows - b.y - b.height};
    const bool crop_inside = after_croped.width > 0 && after_croped.height > 0 &&
                             after_croped.x + after_croped.width <= cadr.cols &&
                             after_croped.y + after_croped.height <= cadr.rows;
    if (!crop_inside) {
      NoteFailure(tape_scanner::ScanStopReason::DeviceFault, "crop is outside the frame");
      return;
    }
    if (!m_session) {
      NoteFailure(tape_scanner::ScanStopReason::DeviceFault, "scan session is not started");
      return;
    }
    // Биннинг после кропа и до сшивки. Шаги моторов не меняются:
    // поле кадра то же, мельче только сохранённая сетка.
    cv::Mat binned = ApplyScanBinning(cadr(after_croped), m_geometry.bin);
    const cv::Point cur_pixel_coord = GetCurPixelCoords();
    if (binned.empty() || !m_session->PushRegion(binned, cur_pixel_coord.x, cur_pixel_coord.y)) {
      NoteFailure(tape_scanner::ScanStopReason::PushFailed, "failed to bin or push the tile");
      return;
    }
  } else {
    if(m_light_only_when_capture)
      m_controller.LightEnable(true);

    AccumCalibrates(m_cur_camera_pos, m_camera.GetCadrWithExposes(m_calibrates.flat_exposes));

    if(m_light_only_when_capture)
      m_controller.LightEnable(false);
  }
  } catch (const std::exception &e) {
    NoteFailure(m_calibrate_running ? tape_scanner::ScanStopReason::Exception
                                    : tape_scanner::ScanStopReason::HdrFailed,
                e.what());
    return;
  }

  wxLogMessage(wxString::Format("CaptureCadr done"));
}

cv::Point Scanner::GetCurPixelCoords() {
    int x = (m_coefs.camera_max_pos / m_coefs.x_steps_in_pixsel -
             m_cur_camera_pos / m_coefs.x_steps_in_pixsel);
    int y = (m_cur_roller_pos) / m_coefs.y_steps_in_pixsel;
    const int bin = m_geometry.bin >= 1 ? m_geometry.bin : 1;
    return {x / bin, y / bin};
}

void Scanner::InitCameraCalibrate() {
    cv::FileStorage fs(R"(./calibr.xml)", cv::FileStorage::READ);
    fs["mat"] >> m_calibrate;
    fs.release();
}

void Scanner::InitScannerCalibrate() {
    cv::FileStorage fs;
    try {
        if (!fs.open(COEFS_FILE_NAME, cv::FileStorage::READ)) {
            wxLogError(wxString::Format("InitScannerCalibrate: Cannot open %s",
                                        COEFS_FILE_NAME));
            return;
        }
    } catch (...) {
        wxLogError(wxString::Format("InitScannerCalibrate: parse error"));
        return;
    }

    for (const auto &pos : fs.root().keys()) {
      if (pos[0] != 'X')
        continue;

      std::stringstream ss(pos.substr(1));
      int pos_int;
      ss >> pos_int;

      cv::Mat coefs;
      fs[pos] >> coefs;

      m_calibrates.coefs_by_pos.insert({pos_int, coefs});
    }

    fs.release();
}


void Scanner::AccumCalibrates(int pos, const cv::Mat& cadr) {
    if (!m_calibrates.cadres_by_pos.contains(pos)) {
        m_calibrates.cadres_by_pos.insert({pos, {}});
    }

    m_calibrates.sum_aver += cv::mean(cadr);
    m_calibrates.total_cadres++;
    auto& list = m_calibrates.cadres_by_pos[pos];
    list.push_back(cadr);
}

void Scanner::CalculateCalibrates() {
    cv::Scalar aver = m_calibrates.sum_aver / m_calibrates.total_cadres;

    cv::FileStorage fs(COEFS_FILE_NAME, cv::FileStorage::WRITE);
    fs << "expose_flat" << m_camera.GetExpose();

    for (const auto& [pos, cadres] : m_calibrates.cadres_by_pos) {
        cv::Mat sum_cadres(m_camera.GetMat().size(), CV_32FC1, cv::Scalar(0));
        for (const auto& cadr : cadres) {
            sum_cadres += cadr;
        }
        sum_cadres /= cadres.size();
        sum_cadres = 1.0 / sum_cadres;
        fs << "X" + std::to_string(pos) << sum_cadres;

        m_calibrates.coefs_by_pos.insert({pos, sum_cadres});
    }

    fs.release();
}

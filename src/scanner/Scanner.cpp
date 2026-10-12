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
  StopScan();
}

void Scanner::StartScan(const std::string& dst_scan_file_path, bool async) {
  wxLogMessage("StartScan");

  if (m_running) {
    wxLogWarning("Scanning already started!");
    return;
  }

    StopScan();

    m_dst_scan_file_path = dst_scan_file_path;
    m_session = std::make_unique<ScanSession>(m_dst_scan_file_path, m_geometry);
    m_running = true;
    m_foce_canceled_unsafe = false;
    m_cur_dir = -1;

    if (async) {
        m_thread = std::make_unique<std::thread>(&Scanner::WorkerLoop, this);
    } else {
        WorkerLoop();
    }
}

void Scanner::StopScan() {
  wxLogMessage("StopScan requested");
  m_running = false;
  if (m_thread && m_thread->joinable()) {
    wxLogMessage("Wait for stop thread");
    m_thread->join();
  }
  wxLogMessage("Scan stopped");
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
  // if (!wxIsMainThread()) {
  //     wxLogStderr* logTarget = new wxLogStderr();
  //     wxLog::SetThreadActiveTarget(logTarget);
  //     wxLog::SetTimestamp("[%H:%M:%S,%l]");
  // }

  m_controller.GoCameraToHome(true);
  if (!WaitFilm()) {
    if (m_session) {
      m_session->Finish();
    }
    return;
  }

  if(!m_light_only_when_capture)
    m_controller.LightEnable(true);

  ResetRollersPosition();
  m_controller.GoRollersTo(16000, true);
  // m_camera.SetExpose(DetectOptimalExpose());
  GoCameraToStartPosition();

  while (!IsCanceled()) {
    CaptureCadr();
    // DebugShowCurFilm();
    GoCameraToNextPosition();
  }

  if(m_light_only_when_capture)
    m_controller.LightEnable(false);

  if (!m_session || !m_session->Finish()) {
    wxLogError("DICOM segment was not saved");
  } else if (m_session->SavedSegmentCount() == 0) {
    wxLogMessage("Scan finished with no pixels to save");
  } else {
    wxLogMessage(wxString::Format("DICOM segments saved: %i", m_session->SavedSegmentCount()));
  }

  m_cur_roller_pos += 40000;
  m_controller.GoRollersTo(m_cur_roller_pos, true);

  m_controller.GoCameraToHome(true);

  m_running = false;

  wxLogMessage("Scan finished");
}

bool Scanner::WaitFilm() {
    while (!HasFilm()) {
        if (IsCanceled())
            return false;
    }
    return true;
}

bool Scanner::IsCanceled() {
    return m_foce_canceled_unsafe || !m_running;
}

void Scanner::Cancel() {
    m_foce_canceled_unsafe = true;
}

bool Scanner::HasFilm() {
    return m_controller.HasFilm() || m_calibrate_running;
}

bool Scanner::HasFilmInCamera() {
    throw std::logic_error("Not implemented!");
}

void Scanner::GoCameraToStartPosition() {
    m_cur_camera_pos = m_coefs.camera_max_pos;
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
    if (m_calibrate_running && m_cur_roller_pos >= m_calibrates.iter_cnt) {
        Cancel();
        return;
    }

    if (!HasFilm()) {
        Cancel();
        return;
    }

    auto old_pos = m_cur_roller_pos;
    m_cur_roller_pos += m_coefs.roller_step_size;

    if (old_pos != m_cur_roller_pos) {
        m_controller.GoRollersTo(m_cur_roller_pos, true);
    }
}

void Scanner::GoCameraToNextPosition() {
    int old_pos = m_cur_camera_pos;
    m_cur_camera_pos += m_cur_dir * m_coefs.camera_step_size;

    if (m_cur_camera_pos > m_coefs.camera_max_pos ||
        m_cur_camera_pos < m_coefs.camera_min_pos) {
        m_cur_dir *= -1;
        m_cur_camera_pos = std::min(m_cur_camera_pos, m_coefs.camera_max_pos);
        m_cur_camera_pos = std::max(m_cur_camera_pos, m_coefs.camera_min_pos);

        m_controller.GoCameraTo(m_cur_camera_pos, true);
        CaptureCadr();

        GoFilmToNextRow();
        if (HasFilm())
            CaptureCadr();

        m_cur_camera_pos += m_cur_dir * m_coefs.camera_step_size;
    }
    if (old_pos != m_cur_camera_pos)
        m_controller.GoCameraTo(m_cur_camera_pos, true);
}

void Scanner::CaptureCadr(bool need_save) {
  wxLogMessage("CaptureCadr start");

  cv::Mat cadr; // result
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
  } else {
    wxLogMessage("WO calibrate");
  }

  if (!m_calibrate_running) {
    if (cadr.empty() || cadr.type() != CV_16UC1) {
      wxLogError("CaptureCadr: empty or unexpected frame");
    } else {
      auto &b = m_coefs.border;
      cv::Rect after_croped = {b.x, b.y, cadr.cols - b.x - b.width, cadr.rows - b.y - b.height};
      const bool crop_inside = after_croped.width > 0 && after_croped.height > 0 &&
                               after_croped.x + after_croped.width <= cadr.cols &&
                               after_croped.y + after_croped.height <= cadr.rows;
      if (!crop_inside) {
        wxLogError("CaptureCadr: crop is outside the frame");
      } else if (!m_session) {
        wxLogError("CaptureCadr: scan session is not started");
      } else {
        try {
          // Биннинг после кропа и до сшивки. Шаги моторов не меняются:
          // поле кадра то же, мельче только сохранённая сетка.
          cv::Mat binned = ApplyScanBinning(cadr(after_croped), m_geometry.bin);
          const cv::Point cur_pixel_coord = GetCurPixelCoords();
          if (binned.empty() || !m_session->PushRegion(binned, cur_pixel_coord.x, cur_pixel_coord.y)) {
            wxLogError("CaptureCadr: failed to bin or push the tile");
          }
        } catch (const std::exception &e) {
          wxLogError(wxString::Format("CaptureCadr: %s", wxString(e.what())));
        }
      }
    }
  } else {
    if(m_light_only_when_capture)
      m_controller.LightEnable(true);

    AccumCalibrates(m_cur_camera_pos, m_camera.GetCadrWithExposes(m_calibrates.flat_exposes));

    if(m_light_only_when_capture)
      m_controller.LightEnable(false);
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

#pragma once

#include "Camera.hpp"
#include "Controller.hpp"
#include "ScanGeometry.hpp"
#include "ScanStop.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <utility>
#include <unordered_map>

class ScanSession;

class Scanner {
public:
    enum class Status { waiting, running, interrupted };

    Scanner(Controller& controller, Camera& camera);
    ~Scanner();

    void StartScan(const std::string& dst_scan_file_path, bool async = true);
    void StopScan();
    /// Флаг отмены и ожидание потока не дольше stop_deadline. Не зависает в join.
    tape_scanner::ScanStopResult RequestStop();
    tape_scanner::ScanRunResult Result() const;
    void SetStopSettings(tape_scanner::ScanStopSettings settings) { m_stop_settings = std::move(settings); }
    bool IsRunning();

    void AutoCalibrateCamera();
    void CalibrateCamera();
    void CalibrateScanner();
    void SetLightOnlyWhenCapture(bool val){
      m_light_only_when_capture = val;
    }

    void SetStepsByPerPixel(double x, double y){
      if (x > 0)
        m_coefs.x_steps_in_pixsel = x;
      if(y > 0)
        m_coefs.y_steps_in_pixsel = y;
    }

    /// Применяется со следующего StartScan. Шаги моторов не меняет.
    void SetScanGeometry(ScanGeometry geometry) { m_geometry = ScanGeometry::Normalized(geometry); }

private:
    void WorkerLoop();
    void RunWorker();
    void FinishTerminal(tape_scanner::TerminalInput input);
    tape_scanner::TerminalInput MakeInput(tape_scanner::ScanStopReason reason, std::string message) const;
    void NoteFailure(tape_scanner::ScanStopReason reason, const std::string &message);
    bool WaitFilm();
    bool IsCanceled();
    bool HasFilm();
    bool HasFilmInCamera();
    void GoCameraToCentre();
    void GoCameraToStartPosition();
    void ResetRollersPosition();
    void GoCameraToNextPosition();
    void GoFilmToNextRow();
    void CaptureCadr(bool need_save = true);
    cv::Point GetCurPixelCoords();
    void InitCameraCalibrate();
    void InitScannerCalibrate();
    void AccumCalibrates(int pos, const cv::Mat& cadr);
    void CalculateCalibrates();

    struct Coefficients {
        int camera_min_pos = 48200;
        int camera_max_pos = 69834;
        int strip_size = 96;
        float x_steps_in_pixsel = 7.0017f;
        float y_steps_in_pixsel = 7.09904f;
        cv::Rect border = {0, 80, 0, 10};
        int overlap_x = 70;
        int overlap_y = 530;
        int camera_step_size = (1280 - border.x - border.width - overlap_x) * x_steps_in_pixsel;
        int roller_step_size = (960 - border.y - border.height - overlap_y) * y_steps_in_pixsel;
    } m_coefs;

    struct Calibrate {
      std::vector<float> flat_exposes = {16, 32, 80};
      std::unordered_map<int, std::vector<cv::Mat>> cadres_by_pos = {};
      cv::Scalar sum_aver = 0;
      int total_cadres = 0;
      std::unordered_map<int, cv::Mat> coefs_by_pos = {};
      int iter_cnt = 0;
    } m_calibrates;

    Controller& m_controller;
    Camera& m_camera;
    std::unique_ptr<ScanSession> m_session;
    ScanGeometry m_geometry;
    tape_scanner::ScanStopSettings m_stop_settings;
    tape_scanner::ScanStopReason m_reason = tape_scanner::ScanStopReason::None;
    std::string m_fail_message;
    bool m_axes_live = false;
    bool m_film_wait_timed_out = false;
    std::atomic<bool> m_terminal_done{false};
    mutable std::mutex m_wait_mu;
    std::condition_variable m_worker_cv;
    bool m_worker_active = false;
    bool m_thread_finished = false;
    std::thread::id m_worker_id;
    tape_scanner::ScanRunResult m_result;
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_operator_stop{false};
    int m_cur_camera_pos = -1;
    int m_cur_roller_pos = 0;
    int m_cur_dir = -1;
    std::unique_ptr<std::thread> m_thread;
    cv::Mat m_calibrate;
    bool m_calibrate_running = false;
    std::string m_dst_scan_file_path;
    bool m_light_only_when_capture = false; // Включать подсветку только при захвате кадра.
};

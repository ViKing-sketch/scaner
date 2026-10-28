#pragma once

#include "Camera.hpp"
#include "Controller.hpp"
#include "ScanGeometry.hpp"
#include <atomic>
#include <memory>
#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <unordered_map>
#include <vector>

class ScanSession;

class Scanner {
public:
    enum class Status { waiting, running, interrupted };

    Scanner(Controller& controller, Camera& camera);
    ~Scanner();

    void StartScan(const std::string& dst_scan_file_path, bool async = true);
    void StopScan();
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
    bool WaitFilm();
    bool IsCanceled();
    void Cancel();
    bool HasFilm();
    bool HasFilmInCamera();
    void GoCameraToCentre();
    void GoCameraToStartPosition();
    void ResetRollersPosition();
    void GoCameraToNextPosition();
    void GoFilmToNextRow();
    void CaptureCadr(bool need_save = true);
    /// Запрошенные микросекунды текущего прохода. При калибровке — flat_exposes.
    const std::vector<float> &ActiveExposures() const;
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
    std::atomic<bool> m_running = false;
    bool m_foce_canceled_unsafe = false;
    int m_cur_camera_pos = -1;
    int m_cur_roller_pos = 0;
    int m_cur_dir = -1;
    std::unique_ptr<std::thread> m_thread;
    cv::Mat m_calibrate;
    bool m_calibrate_running = false;
    std::string m_dst_scan_file_path;
    bool m_light_only_when_capture = false; // Включать подсветку только при захвате кадра.
};

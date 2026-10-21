#pragma once

#include <wx/wx.h>

#include <wx/log.h>
#include <wx/msw/bitmap.h>

#include <camera_api/CameraApi.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <minwindef.h>
#include <opencv2/opencv.hpp>
#include <stdexcept>

#include <tape_scanner/scanner/LinkLoss.hpp>


class Camera {
public:
    Camera();
    ~Camera();

    bool TryConnect();
    /// ConnectTest, затем CameraReConnect, затем полный перебор, если рукоять умерла.
    bool TryReconnect();
    /// true, пока CameraConnectTest видит камеру. Связь не поднимает.
    bool Probe();
    bool IsConnected();

    void EnableAutoExpose(bool enable = true);
    void SetExpose(int microseconds = 500);
    double GetExpose();

    uint8_t* GetCadr(tSdkFrameHead& frame_info, double expose = 0.0);
    void SaveCadrTo(const std::string& dest_path);
    wxBitmap Snapshot(double expose = 0.0);
    cv::Mat GetMat(double expose = 0.0);
    cv::Mat GetCadrWithExposes(std::vector<float> exposes);
    void CalibrateResponses();

  private:
    int GetFirstCamera();
    void InitResponses();
    void OpenFirstUnlocked();
    void ReleaseUnlocked();
    bool GrabIsLinkLoss(CameraSdkStatus status);
    cv::Mat ToRadiance(std::vector<cv::Mat> imgs_16, std::vector<float> exposes);

    int m_h_camera = -1;
    bool m_link_down = false;
    uint8_t* m_frame_buffer = nullptr;
    uint8_t* m_output_frame_buffer = nullptr;
    bool m_is_mono = false;
    size_t m_pixel_size = 0;
    size_t m_height = 0;
    size_t m_width = 0;
    cv::Mat m_responses;

    std::mutex m_mutex;
};

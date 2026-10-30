#pragma once

#include "tape_scanner/dicom_viewer/GLImgViewer.h"
#include <tape_scanner/scanner/DicomScan.hpp>
#include <tape_scanner/scanner/DicomSeries.hpp>
#include <tape_scanner/scanner/WindowLevel.hpp>
#include <string>
#include <vector>

class DicomProcessor
{
public:
    DicomProcessor(GLImgViewer& gl_viewer);

    void Open(const wxString& file_name);
    int NextFrame();
    int PrevFrame();
    int SetFrame(int frame_num = 0);
    void SetWindow(double center, double width) { m_gl_viewer.SetWindow(center, width); }
    void SetInvert(bool invert) { m_gl_viewer.SetInvert(invert); }
    void SetOrientation(int rotation_deg, int flip) { m_gl_viewer.SetOrientation(rotation_deg, flip); }
    WindowLevel CurrentWindow() const { return m_gl_viewer.CurrentWindow(); }
    bool Inverted() const { return m_gl_viewer.Inverted(); }
    const DicomScan &Current() const { return m_scan; }
    size_t GetFramesCount();
    size_t GetFrameHeight() const { return m_scan.rows > 0 ? static_cast<size_t>(m_scan.rows) : 0; }
    size_t GetFrameWidth() const { return m_scan.cols > 0 ? static_cast<size_t>(m_scan.cols) : 0; }
    int GetCurFrameNum() const { return m_series.size() > 1 ? m_series_index : m_cur_frame; }
private:
    GLImgViewer& m_gl_viewer;
    DicomScan m_scan;
    std::vector<DicomSeriesPart> m_series;
    int m_series_index = 0;
    int m_cur_frame = 0;
    bool m_reset_view = false;
    std::string m_current_path;

    bool ShowSeriesFrame(int index);
    cv::Mat GetCurFrame();
    void Show(bool reset_view);
};

#pragma once

#include "tape_scanner/dicom_viewer/GLImgViewer.h"
#include <tape_scanner/scanner/DicomSeries.hpp>
#include <tape_scanner/scanner/WindowLevel.hpp>
#include <memory>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <dcmtk/dcmjpeg/djcodecd.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include "dcmtk/dcmimage/diregist.h"

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
    size_t GetFramesCount();
    size_t GetFrameHeight(){ return m_dcm ? m_dcm->getHeight() : 0; }
    size_t GetFrameWidth(){ return m_dcm ? m_dcm->getWidth() : 0; }
    int GetCurFrameNum() const { return m_series.size() > 1 ? m_series_index : m_cur_frame; }
private:
    GLImgViewer& m_gl_viewer;
    std::unique_ptr<DicomImage> m_dcm;
    std::vector<DicomSeriesPart> m_series;
    int m_series_index = 0;
    int m_cur_frame = 0;
    bool m_reset_view = false;
    std::string m_current_path;

    bool ShowSeriesFrame(int index);

    void OpenDicomImage(const wxString& path_to_dcm_file);
    cv::Mat GetCurFrame();
    void Show(bool reset_view);
};
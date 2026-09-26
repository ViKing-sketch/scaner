#pragma once

#include "tape_scanner/dicom_viewer/GLImgViewer.h"
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
    void SetBright(float bright){ m_gl_viewer.SetBright(bright); };
    void SetContrast(float contrast){ m_gl_viewer.SetContrast(contrast); };
    size_t GetFramesCount(){ return m_dcm ? m_dcm->getNumberOfFrames() : 0; }
    size_t GetFrameHeight(){ return m_dcm ? m_dcm->getHeight() : 0; }
    size_t GetFrameWidth(){ return m_dcm ? m_dcm->getWidth() : 0; }
    int GetCurFrameNum(){ return m_cur_frame; }
private:
    GLImgViewer& m_gl_viewer;
    std::unique_ptr<DicomImage> m_dcm;
    int m_cur_frame = 0;

    void OpenDicomImage(const wxString& path_to_dcm_file);
    cv::Mat GetCurFrame();
    void Show();
};
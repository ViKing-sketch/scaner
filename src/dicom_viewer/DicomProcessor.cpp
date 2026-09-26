#include "tape_scanner/dicom_viewer/DicomProcessor.h"
#include "tape_scanner/utils/Logger.h"
#include <stdexcept>

DicomProcessor::DicomProcessor(GLImgViewer &gl_viewer)
    : m_gl_viewer(gl_viewer)
{
}

void DicomProcessor::Open(const wxString& file_name)
{
    OpenDicomImage(file_name);    
    m_cur_frame = 0;
    Show();
}

int DicomProcessor::NextFrame()
{
    if(!m_dcm)
        return m_cur_frame;

    ++m_cur_frame;
    
    if(m_cur_frame == m_dcm->getFrameCount())
        m_cur_frame = 0;

    Show();
    return m_cur_frame;
}

int DicomProcessor::PrevFrame()
{
    if(!m_dcm)
        return m_cur_frame;

    --m_cur_frame;
    
    if(m_cur_frame == -1)
        m_cur_frame = m_dcm->getFrameCount() - 1;

    Show();
    return m_cur_frame;
}

int DicomProcessor::SetFrame(int frame_num)
{
    if(!m_dcm)
        return m_cur_frame;

    m_cur_frame = std::min(m_dcm->getFrameCount() - 1, static_cast<unsigned long>(std::max(frame_num, 0)));

    Show();
    return m_cur_frame;
}

void DicomProcessor::OpenDicomImage(const wxString& path_to_dcm_file)
{
    get_logger() << "Open with AUTO decode \n";
    int res = 0;

    DJDecoderRegistration::registerCodecs();

    m_dcm = std::make_unique<DicomImage>(path_to_dcm_file.ToStdString().c_str());
    
    if(!m_dcm)
        throw std::logic_error("Error open file: " + path_to_dcm_file);
    if(m_dcm->getStatus() != EIS_Normal) 
        throw std::logic_error("Error: status is not normal: " + path_to_dcm_file);
}

cv::Mat DicomProcessor::GetCurFrame()
{
    if(!m_dcm)
        return cv::Mat();

    m_dcm->setMinMaxWindow();

    int depth_bits = m_dcm->getDepth() == 8 ? 8 : 16;
    int depth_bytes = depth_bits/8;
    int type = CV_MAKETYPE((m_dcm->isMonochrome() ? CV_8U : CV_8U), depth_bytes);

    if(m_dcm->isMonochrome() && depth_bits == 16)
        type = CV_16UC1;
    else if(m_dcm->isMonochrome() && depth_bits == 8)
        type = CV_8UC1;
    else if(!m_dcm->isMonochrome() && depth_bits == 8)
        type = CV_8UC3;
    else
        throw std::logic_error("Error: unsupported image in dcm file!");
    
    cv::Mat img(m_dcm->getHeight(), m_dcm->getWidth(), type, (void*)(m_dcm->getOutputData(depth_bits, m_cur_frame)));
    return img;
}

void DicomProcessor::Show()
{
    m_gl_viewer.SetImage(GetCurFrame());
}

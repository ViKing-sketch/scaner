#include "tape_scanner/dicom_viewer/DicomProcessor.h"
#include "tape_scanner/utils/Logger.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <stdexcept>

DicomProcessor::DicomProcessor(GLImgViewer &gl_viewer)
    : m_gl_viewer(gl_viewer)
{
}

void DicomProcessor::Open(const wxString& file_name)
{
    m_series = ListDicomSeries(file_name.ToStdString());
    m_series_index = 0;
    m_cur_frame = 0;
    m_reset_view = true;
    if (m_series.size() > 1) {
        const std::filesystem::path opened(file_name.ToStdString());
        for (int i = 0; i < static_cast<int>(m_series.size()); ++i) {
            std::error_code error;
            if (std::filesystem::equivalent(opened, m_series[static_cast<size_t>(i)].path, error)) {
                m_series_index = i;
                break;
            }
        }
        ShowSeriesFrame(m_series_index);
        return;
    }
    OpenDicomImage(file_name);
    Show(true);
    m_reset_view = false;
}

bool DicomProcessor::ShowSeriesFrame(int index)
{
    if (index < 0 || index >= static_cast<int>(m_series.size())) {
        return false;
    }
    m_series_index = index;
    m_cur_frame = 0;
    OpenDicomImage(wxString(m_series[static_cast<size_t>(index)].path.c_str(), wxConvLocal));
    Show(m_reset_view);
    m_reset_view = false;
    return true;
}

size_t DicomProcessor::GetFramesCount()
{
    if (m_series.size() > 1) {
        return m_series.size();
    }
    return m_dcm ? m_dcm->getNumberOfFrames() : 0;
}

int DicomProcessor::NextFrame()
{
    if (m_series.size() > 1) {
        const int next = (m_series_index + 1) % static_cast<int>(m_series.size());
        ShowSeriesFrame(next);
        return m_series_index;
    }
    if(!m_dcm)
        return m_cur_frame;

    ++m_cur_frame;
    
    if(m_cur_frame == m_dcm->getFrameCount())
        m_cur_frame = 0;

    Show(false);
    return m_cur_frame;
}

int DicomProcessor::PrevFrame()
{
    if (m_series.size() > 1) {
        const int prev = m_series_index == 0 ? static_cast<int>(m_series.size()) - 1 : m_series_index - 1;
        ShowSeriesFrame(prev);
        return m_series_index;
    }
    if(!m_dcm)
        return m_cur_frame;

    --m_cur_frame;
    
    if(m_cur_frame == -1)
        m_cur_frame = m_dcm->getFrameCount() - 1;

    Show(false);
    return m_cur_frame;
}

int DicomProcessor::SetFrame(int frame_num)
{
    if (m_series.size() > 1) {
        const int last = static_cast<int>(m_series.size()) - 1;
        const int index = std::min(last, std::max(frame_num, 0));
        ShowSeriesFrame(index);
        return m_series_index;
    }
    if(!m_dcm)
        return m_cur_frame;

    m_cur_frame = std::min(m_dcm->getFrameCount() - 1, static_cast<unsigned long>(std::max(frame_num, 0)));

    Show(false);
    return m_cur_frame;
}

void DicomProcessor::OpenDicomImage(const wxString& path_to_dcm_file)
{
    get_logger() << "Open with AUTO decode \n";
    int res = 0;

    DJDecoderRegistration::registerCodecs();

    m_current_path = path_to_dcm_file.ToStdString();
    m_dcm = std::make_unique<DicomImage>(m_current_path.c_str());
    
    if(!m_dcm)
        throw std::logic_error("Error open file: " + path_to_dcm_file);
    if(m_dcm->getStatus() != EIS_Normal) 
        throw std::logic_error("Error: status is not normal: " + path_to_dcm_file);
}

cv::Mat DicomProcessor::GetCurFrame()
{
    if(!m_dcm)
        return cv::Mat();

    cv::Mat stored = ReadStoredMono16(m_current_path, m_cur_frame);
    if (!stored.empty()) {
        return stored;
    }

    m_dcm->setNoVoiTransformation();

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
    
    const void *data = m_dcm->getOutputData(depth_bits, m_cur_frame);
    if (!data) {
        throw std::logic_error("Error: no pixel data");
    }
    cv::Mat img(m_dcm->getHeight(), m_dcm->getWidth(), type);
    std::memcpy(img.data, data, img.total() * img.elemSize());
    return img;
}

void DicomProcessor::Show(bool reset_view)
{
    cv::Mat frame = GetCurFrame();
    if (reset_view) {
        const auto [min_value, max_value] = RawMinMax(frame);
        const WindowLevel fitted = WindowFromMinMax(min_value, max_value);
        m_gl_viewer.SetWindow(fitted.center, fitted.width);
        m_gl_viewer.SetInvert(false);
        m_gl_viewer.RememberOrientation(0, 0);
    }
    m_gl_viewer.SetImage(frame, reset_view);
}

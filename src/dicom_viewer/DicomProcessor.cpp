#include "tape_scanner/dicom_viewer/DicomProcessor.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>

DicomProcessor::DicomProcessor(GLImgViewer &gl_viewer)
    : m_gl_viewer(gl_viewer)
{
}

void DicomProcessor::Open(const wxString& file_name)
{
    const std::string path = file_name.ToStdString();
    auto series = ListDicomSeries(path);
    int index = 0;
    std::string image_path = path;
    if (series.size() > 1) {
        const std::filesystem::path opened(path);
        for (int i = 0; i < static_cast<int>(series.size()); ++i) {
            std::error_code error;
            if (std::filesystem::equivalent(opened, series[static_cast<size_t>(i)].path, error) && !error) {
                index = i;
                break;
            }
        }
        image_path = series[static_cast<size_t>(index)].path;
    }

    DicomScan scan;
    std::string error;
    if (!LoadDicomScan(image_path, scan, error)) {
        throw std::logic_error(error.empty() ? "Error open file: " + image_path : error);
    }

    m_series = std::move(series);
    m_series_index = index;
    m_cur_frame = 0;
    m_scan = std::move(scan);
    m_current_path = image_path;
    m_reset_view = true;
    Show(true);
    m_reset_view = false;
}

bool DicomProcessor::ShowSeriesFrame(int index)
{
    if (index < 0 || index >= static_cast<int>(m_series.size())) {
        return false;
    }
    const std::string path = m_series[static_cast<size_t>(index)].path;
    DicomScan scan;
    std::string error;
    if (!LoadDicomScan(path, scan, error)) {
        throw std::logic_error(error.empty() ? "Error open file: " + path : error);
    }
    m_series_index = index;
    m_cur_frame = 0;
    m_scan = std::move(scan);
    m_current_path = path;
    Show(m_reset_view);
    m_reset_view = false;
    return true;
}

size_t DicomProcessor::GetFramesCount()
{
    if (m_series.size() > 1) {
        return m_series.size();
    }
    return m_scan.frames.size();
}

int DicomProcessor::NextFrame()
{
    if (m_series.size() > 1) {
        const int next = (m_series_index + 1) % static_cast<int>(m_series.size());
        ShowSeriesFrame(next);
        return m_series_index;
    }
    const int count = static_cast<int>(m_scan.frames.size());
    if (count <= 0) {
        return m_cur_frame;
    }
    ++m_cur_frame;
    if (m_cur_frame >= count) {
        m_cur_frame = 0;
    }
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
    const int count = static_cast<int>(m_scan.frames.size());
    if (count <= 0) {
        return m_cur_frame;
    }
    --m_cur_frame;
    if (m_cur_frame < 0) {
        m_cur_frame = count - 1;
    }
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
    const int count = static_cast<int>(m_scan.frames.size());
    if (count <= 0) {
        return m_cur_frame;
    }
    m_cur_frame = std::min(count - 1, std::max(frame_num, 0));
    Show(false);
    return m_cur_frame;
}

cv::Mat DicomProcessor::GetCurFrame()
{
    if (m_cur_frame < 0 || m_cur_frame >= static_cast<int>(m_scan.frames.size())) {
        return {};
    }
    return m_scan.frames[static_cast<size_t>(m_cur_frame)];
}

void DicomProcessor::Show(bool reset_view)
{
    cv::Mat frame = GetCurFrame();
    if (frame.empty()) {
        return;
    }
    if (reset_view) {
        int min_value = 0;
        int max_value = frame.type() == CV_16UC1 ? 65535 : 255;
        if (frame.type() == CV_16UC1 || frame.type() == CV_8UC1) {
            const auto range = RawMinMax(frame);
            min_value = range.first;
            max_value = range.second;
        }
        const WindowLevel fitted = WindowFromMinMax(min_value, max_value);
        m_gl_viewer.SetWindow(fitted.center, fitted.width);
        m_gl_viewer.SetInvert(m_scan.monochrome1);
        m_gl_viewer.RememberOrientation(0, 0);
    }
    m_gl_viewer.SetImage(frame, reset_view);
}

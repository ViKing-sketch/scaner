#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <utility>

// Constant for image type used throughout
inline constexpr int MAT_IMG_TYPE = CV_16UC1;

class Region {
public:
    Region() = default;

    Region(int x, int y, int h = 0, int w = 0);

    Region(int x, int y, const cv::Mat &img);

    void InsertRegion(const Region &region);

    /// Глубокая копия холста: последующая вставка не меняет исходный Region.
    Region Clone() const;

    cv::Mat &GetMat() const;

    cv::Rect GetRect() const;

    cv::Rect GetPastedRegion() const;

private:
    std::pair<bool, std::string> IsInsertable(const Region &region);

    cv::Mat GetRoi(cv::Rect roi) const;

    cv::Rect GetIntersect(cv::Rect region);

    cv::Rect GetNonIntersect(cv::Rect region);

    cv::Mat GetOverlap(cv::Rect intersect, const Region &region);

    cv::Mat GetNonOverlap(cv::Rect non_intersect, const Region &region);

    void Expand(cv::Rect insertable);

    cv::Rect ExpandPastedRegion(cv::Rect insertable);

    int m_x = 0;
    int m_y = 0;
    mutable cv::Mat m_canvas;
    cv::Rect m_pasted_region = {0, 0, 0, 0};
};

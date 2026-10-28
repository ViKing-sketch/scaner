#include <tape_scanner/scanner/Coverage.hpp>
#include <tape_scanner/scanner/Region.hpp>

namespace {

cv::Mat blend(cv::Mat prev_part, cv::Mat new_part, bool horizontal = true,
              bool is_reverse = false) {
    if (prev_part.size() != new_part.size()) {
        throw std::logic_error("blend: parts sizes must be the same!");
    }

    cv::Mat blend_weights(prev_part.size(), CV_32FC1);

    for (int y = 0; y < blend_weights.rows; ++y) {
        for (int x = 0; x < blend_weights.cols; ++x) {
            int a = horizontal ? x : y;
            int b = horizontal ? blend_weights.cols : blend_weights.rows;
            float alpha = 1.0f - static_cast<float>(a) / b;

            blend_weights.at<float>(y, x) = alpha;
        }
    }

    cv::Mat prev_part_float;
    cv::Mat new_part_float;
    prev_part.convertTo(prev_part_float, CV_32FC1);
    new_part.convertTo(new_part_float, CV_32FC1);

    cv::Mat *left_or_top = &prev_part_float;
    cv::Mat *right_or_bottom = &new_part_float;

    if (is_reverse)
        std::swap(left_or_top, right_or_bottom);

    cv::Mat blended = left_or_top->mul(blend_weights) +
                      right_or_bottom->mul(1.0f - blend_weights);

    cv::Mat res;
    blended.convertTo(res, MAT_IMG_TYPE);

    return res;
}

} // anonymous namespace

Region::Region(int x, int y, int h, int w)
    : m_x(x), m_y(y), m_canvas(h, w, MAT_IMG_TYPE, cv::Scalar(0)),
      m_valid(h, w, CV_8UC1, cv::Scalar(kCoverageUnfilled)) {
    m_pasted_region = {0, 0, 0, 0};
}

Region::Region(int x, int y, const cv::Mat &img) : Region(x, y, img, cv::Mat()) {}

Region::Region(int x, int y, const cv::Mat &img, const cv::Mat &coverage)
    : m_x(x), m_y(y), m_canvas(img.clone()) {
    if (coverage.empty()) {
        m_valid = cv::Mat(m_canvas.rows, m_canvas.cols, CV_8UC1, cv::Scalar(kCoverageValid));
    } else if (coverage.type() == CV_8UC1 && coverage.size() == m_canvas.size()) {
        m_valid = coverage.clone();
    } else {
        throw std::logic_error("Region coverage must be CV_8UC1 and match the tile");
    }
    m_pasted_region = {0, 0, img.cols, img.rows};
}

Region Region::Clone() const {
    Region copy(*this);
    copy.m_canvas = m_canvas.clone();
    copy.m_valid = m_valid.clone();
    return copy;
}

void Region::InsertRegion(const Region &region) {
    if (auto [insertable, reason] = IsInsertable(region); !insertable) {
        throw std::logic_error(reason);
    }

    cv::Rect intersect = GetIntersect(region.GetRect());
    cv::Rect non_intersect = GetNonIntersect(region.GetRect());

    Expand(non_intersect);

    if (intersect.area()) {
        cv::Mat overlap = GetOverlap(intersect, region);
        overlap.copyTo(m_canvas(intersect));
        // Смешение с дырой не является измерением. Оба края должны быть заполнены.
        cv::Mat src_valid = region.ValidRoi(intersect);
        cv::Mat dst_valid = m_valid(intersect);
        if (src_valid.size() == dst_valid.size() && src_valid.type() == CV_8UC1) {
            cv::Mat both;
            cv::bitwise_and(dst_valid, src_valid, both);
            both.copyTo(dst_valid);
        } else {
            dst_valid.setTo(kCoverageUnfilled);
        }
    }

    if (non_intersect.area()) {
        cv::Mat non_overlap = GetNonOverlap(non_intersect, region);
        non_overlap.copyTo(m_canvas(non_intersect));
        cv::Mat src_valid = region.ValidRoi(non_intersect);
        if (src_valid.size() == m_valid(non_intersect).size() && src_valid.type() == CV_8UC1) {
            src_valid.copyTo(m_valid(non_intersect));
        } else {
            m_valid(non_intersect).setTo(kCoverageUnfilled);
        }
    }
}

cv::Mat &Region::GetMat() const {
    return m_canvas;
}

const cv::Mat &Region::Valid() const {
    return m_valid;
}

cv::Rect Region::GetRect() const {
    return {m_x, m_y, m_canvas.cols, m_canvas.rows};
}

cv::Rect Region::GetPastedRegion() const {
    return m_pasted_region;
}

std::pair<bool, std::string> Region::IsInsertable(const Region &region) {
    if (region.m_x < 0 || region.m_y < 0) {
        return {false, "Inserting region must be have positive coordinates!"};
    }

    if (region.GetMat().type() != MAT_IMG_TYPE) {
        return {false, "Insertin region image must be have type is MAT_IMG_TYPE!"};
    }

    if (region.GetRect().area() == 0) {
        return {false, "Inserted region must be have positive area!"};
    }

    cv::Rect intersect = GetIntersect(region.GetRect());
    if (intersect.area() && intersect.width != m_pasted_region.width &&
        intersect.height != m_pasted_region.height) {
        return {false, "Only ortogonal insertion is supported!"};
    }

    return {true, ""};
}

cv::Mat Region::GetRoi(cv::Rect roi) const {
    roi.x = std::max(roi.x - m_x, 0);
    roi.y = std::max(roi.y - m_y, 0);
    roi.width = std::min(roi.width, m_canvas.cols);
    roi.height = std::min(roi.height, m_canvas.rows);

    if (roi.area() == 0) {
        throw std::logic_error("Region::GetRoi: roi.area() must be greater 0");
    }

    return m_canvas(roi);
}

cv::Mat Region::ValidRoi(cv::Rect roi) const {
    roi.x = std::max(roi.x - m_x, 0);
    roi.y = std::max(roi.y - m_y, 0);
    roi.width = std::min(roi.width, m_valid.cols);
    roi.height = std::min(roi.height, m_valid.rows);

    if (roi.area() == 0 || m_valid.empty()) {
        throw std::logic_error("Region::ValidRoi: roi.area() must be greater 0");
    }

    return m_valid(roi);
}

cv::Rect Region::GetIntersect(cv::Rect region) {
    return region & m_pasted_region;
}

cv::Rect Region::GetNonIntersect(cv::Rect region) {
    cv::Rect res = region;
    auto intersect = GetIntersect(region);
    bool is_horizontal = intersect.height == region.height;
    if (is_horizontal) {
        res.width -= intersect.width;
        // Сканирование слева --> направо
        if (region.x > m_pasted_region.x) {
            res.x += intersect.width;
        }
    } else {
        res.height -= intersect.height;
        // Сканирование сверху вниз
        if (region.y > m_pasted_region.y) {
            res.y += intersect.height;
        }
    }

    return res;
}

cv::Mat Region::GetOverlap(cv::Rect intersect, const Region &region) {
    bool is_horizontal = intersect.height == region.GetRect().height;
    bool is_reverse = (region.m_x < intersect.x && is_horizontal) ||
                      (region.m_y < intersect.y && !is_horizontal);
    return blend(m_canvas(intersect), region.GetRoi(intersect), is_horizontal,
                 is_reverse);
}

cv::Mat Region::GetNonOverlap(cv::Rect non_intersect, const Region &region) {
    return region.GetRoi(non_intersect);
}

void Region::Expand(cv::Rect insertable) {
    auto old_pasted_region = m_pasted_region;

    m_pasted_region = ExpandPastedRegion(insertable);

    int required_canvas_cols = m_pasted_region.x + m_pasted_region.width;
    int required_canvas_rows = m_pasted_region.y + m_pasted_region.height;

    if (required_canvas_cols <= m_canvas.cols &&
        required_canvas_rows <= m_canvas.rows) {
        return;
    }

    cv::Size new_size = {required_canvas_cols, required_canvas_rows};
    cv::Mat new_canvas(new_size, MAT_IMG_TYPE, cv::Scalar(0));
    cv::Mat new_valid(new_size, CV_8UC1, cv::Scalar(kCoverageUnfilled));
    if (old_pasted_region.area()) {
        m_canvas(old_pasted_region).copyTo(new_canvas(old_pasted_region));
        const cv::Rect known = old_pasted_region & cv::Rect(0, 0, m_valid.cols, m_valid.rows);
        if (known.area() > 0) {
            m_valid(known).copyTo(new_valid(known));
        }
    }
    m_canvas = new_canvas;
    m_valid = new_valid;
}

cv::Rect Region::ExpandPastedRegion(cv::Rect insertable) {
    const auto &before = m_pasted_region;

    if (before.area() == 0) {
        return insertable;
    }

    auto left_x = std::min(before.x, insertable.x);
    auto top_y = std::min(before.y, insertable.y);
    auto right_x = std::max(before.x + before.width, insertable.x + insertable.width);
    auto bottom_y = std::max(before.y + before.height, insertable.y + insertable.height);

    cv::Rect after = {left_x, top_y, right_x - left_x, bottom_y - top_y};
    return after;
}

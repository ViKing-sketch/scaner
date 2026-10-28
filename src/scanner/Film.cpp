#include <tape_scanner/scanner/Film.hpp>
#include <tape_scanner/scanner/ScanLog.hpp>
#include <opencv2/core.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace {

/// Полоса уже есть в холсте по Y и уже по X: вклеиваем только строки строго ниже текущего низа.
void PasteRowsBelow(Region &dest, const Region &stripe) {
    const cv::Rect stripe_rect = stripe.GetRect();
    const cv::Rect dest_rect = dest.GetPastedRegion();
    const int dest_bottom = dest_rect.area() > 0 ? dest_rect.y + dest_rect.height : stripe_rect.y;
    const int row0 = std::max(0, dest_bottom - stripe_rect.y);
    if (row0 >= stripe.GetMat().rows) {
        return;
    }
    cv::Mat rows = stripe.GetMat().rowRange(row0, stripe.GetMat().rows).clone();
    cv::Mat coverage;
    if (stripe.Valid().rows == stripe.GetMat().rows && stripe.Valid().cols == stripe.GetMat().cols) {
        coverage = stripe.Valid().rowRange(row0, stripe.Valid().rows).clone();
    }
    Region piece(stripe_rect.x, stripe_rect.y + row0, rows, coverage);
    dest.InsertRegion(piece);
}

} // namespace

void Film::PushRegion(cv::Mat img, int x, int y, cv::Mat coverage) {
    ScanLogMessageF("Film: Pushed x:%i, y:%i, w:%i, h:%i", x, y, img.cols, img.rows);
    bool need_new_stripe =
        m_cur_stripe.GetRect().area() && y != m_cur_stripe.GetRect().y;

    if (need_new_stripe) {
        m_all_stripes.InsertRegion(m_cur_stripe);
        m_cur_stripe = {0, y};
        ++m_cur_stripe_num;
    } else if (m_cur_stripe.GetRect().area() == 0) {
        // Пустая полоса сбрасывается в {0, 0}. Без этого первый тайл
        // игнорировал бы y и сегмент после Reset съезжал.
        m_cur_stripe = {0, y};
    }

    m_cur_region_num++;
    m_cur_stripe.InsertRegion(Region(x, 0, img, coverage));
}

bool Film::HasOpenStripe() const {
    return m_cur_stripe.GetRect().area() > 0;
}

bool Film::HasContent() const {
    return HasOpenStripe() || m_all_stripes.GetRect().area() > 0;
}

int Film::CurrentStripeY() const {
    return m_cur_stripe.GetRect().y;
}

int Film::ContentRows() const {
    int bottom = 0;
    const auto consider = [&](const cv::Rect &rect) {
        if (rect.area() > 0) {
            bottom = std::max(bottom, rect.y + rect.height);
        }
    };
    consider(m_all_stripes.GetPastedRegion());
    consider(m_all_stripes.GetRect());
    consider(m_cur_stripe.GetRect());
    return bottom;
}

cv::Mat Film::GetMat() {
    // Если хвост полный или он единственный, вставим в общую пленку.
    if (auto pasted = m_cur_stripe.GetPastedRegion();
        pasted.width == m_all_stripes.GetRect().width ||
        m_all_stripes.GetRect().area() == 0) {
        try {
            m_all_stripes.InsertRegion(m_cur_stripe);
        } catch (std::logic_error &e) {
            ScanLogError(std::string("Film: GetMat: ") + e.what());
        }
        // m_cur_stripe = {0, 0};
    }
    ScanLogMessageF("Film: GetMat w:%i, h:%i", m_all_stripes.GetRect().width, m_all_stripes.GetRect().height);
    return m_all_stripes.GetMat();
}

CoveredFrame Film::RenderCovered() const {
    Region canvas = m_all_stripes.Clone();
    if (m_cur_stripe.GetRect().area() > 0) {
        try {
            canvas.InsertRegion(m_cur_stripe);
        } catch (const std::logic_error &) {
            try {
                PasteRowsBelow(canvas, m_cur_stripe);
            } catch (const std::logic_error &e) {
                ScanLogError(std::string("Film: short stripe was not attached: ") + e.what());
            }
        }
    }

    CoveredFrame frame;
    frame.image = canvas.GetMat().clone();
    frame.coverage = canvas.Valid().clone();
    if (frame.coverage.type() != CV_8UC1 || frame.coverage.size() != frame.image.size()) {
        frame.coverage = cv::Mat(frame.image.rows, frame.image.cols, CV_8UC1, cv::Scalar(kCoverageUnfilled));
    }
    frame.stats = IntensityOnCoverage(frame.image, frame.coverage);
    frame.complete = frame.stats.available && frame.stats.invalid_count == 0;
    if (!frame.image.empty() && !frame.complete) {
        frame.gap_reason = kCoverageGapUnfilled;
    }
    return frame;
}

cv::Mat Film::Render() const {
    return RenderCovered().image;
}

void Film::Reset() {
    m_all_stripes = {0, 0};
    m_cur_stripe = {0, 0};
    // Раньше здесь объявлялись локальные переменные с теми же именами,
    // и счётчики объекта после первого сегмента не обнулялись.
    m_cur_region_num = 0;
    m_cur_stripe_num = 0;
}

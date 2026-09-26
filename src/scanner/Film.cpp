#include <tape_scanner/scanner/Film.hpp>
#include <cassert>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/strvararg.h>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>

void Film::PushRegion(cv::Mat img, int x, int y) {
    wxLogMessage(wxString::Format("Film: Pushed x:%i, y:%i, w:%i, h:%i", x, y,
                                  img.cols, img.rows));
    cv::imwrite(
        "cadres/" + std::to_string(m_cur_region_num) + " - " + std::to_string(y) + "_" + std::to_string(x) + ".png", img);
    bool need_new_stripe =
        m_cur_stripe.GetRect().area() && y != m_cur_stripe.GetRect().y;

    if (need_new_stripe) {
        m_all_stripes.InsertRegion(m_cur_stripe);
        m_cur_stripe = {0, y};
    }

    m_cur_region_num++;
    m_cur_stripe.InsertRegion({x, 0, img});
}

cv::Mat Film::GetMat() {
    // Если хвост полный или он единственный, вставим в общую пленку.
    if (auto pasted = m_cur_stripe.GetPastedRegion();
        pasted.width == m_all_stripes.GetRect().width ||
        m_all_stripes.GetRect().area() == 0) {
        try {
            m_all_stripes.InsertRegion(m_cur_stripe);
        } catch (std::logic_error &e) {
            wxLogError("Film: GetMat: " + wxString(e.what()));
        }
        // m_cur_stripe = {0, 0};
    }
    wxLogMessage(wxString::Format("Film: GetMat w:%i, h%i",
                                  m_all_stripes.GetRect().width,
                                  m_all_stripes.GetRect().height));
    return m_all_stripes.GetMat();
}

void Film::Reset() {
    m_all_stripes = {0, 0};
    m_cur_stripe = {0, 0};

    int m_cur_region_num = 0;
    int m_cur_stripe_num = 0;
}

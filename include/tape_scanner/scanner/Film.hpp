#ifndef FILM_HPP
#define FILM_HPP

#include <tape_scanner/scanner/Region.hpp>
#include <tape_scanner/scanner/Utils.hpp>
#include <opencv2/opencv.hpp>

class Film {
public:
    Film() = default;

    void PushRegion(cv::Mat img, int x, int y);

    cv::Mat GetMat();

    bool SaveToDcm(const std::string& dst_file_path){
      return saveToDcm(GetMat(), dst_file_path);
    }

    void Reset();

private:
  Region m_all_stripes;
    Region m_cur_stripe;

    int m_cur_region_num = 0;
    int m_cur_stripe_num = 0;
};

#endif // FILM_HPP

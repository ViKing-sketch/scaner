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

    /// Копия холста вместе с незаконченной полосой, в том числе короче полной ширины.
    /// Исходный Film не меняется: повторный вызов не вклеивает полосу второй раз.
    cv::Mat Render() const;

    bool HasOpenStripe() const;
    bool HasContent() const;
    int CurrentStripeY() const;
    /// Нижняя строка холста в координатах сегмента. Совпадает с числом строк Render(), если холст начинается с 0.
    int ContentRows() const;

    bool SaveToDcm(const std::string& dst_file_path, const DicomWriteParams& params = {}){
      return saveToDcm(Render(), dst_file_path, params);
    }

    void Reset();

private:
  Region m_all_stripes;
    Region m_cur_stripe;

    int m_cur_region_num = 0;
    int m_cur_stripe_num = 0;
};

#endif // FILM_HPP

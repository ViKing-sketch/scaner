#include <tape_scanner/scanner/Region.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <wx/log.h>

constexpr bool SAVE_MODE = false;

int main() {
  constexpr int cadr_cols = 200;
  constexpr int cadr_rows = 100;
  constexpr int cadr_overlap = 50;   // Нахлест кадров
  constexpr int stripe_overlap = 50; // Нахлест полос сканирования
  constexpr int cadr_step = cadr_cols - cadr_overlap;
  constexpr int stripe_step = cadr_rows - stripe_overlap;

  cv::Mat cadr(cadr_rows, cadr_cols, CV_16UC1, cv::Scalar(65000));

  Region film;
  for (int y = 0; y < 4; ++y) {
    Region stripe(0, y * stripe_step, 0, 0);

    for (int x = 0; x < 3; ++x) {
      stripe.InsertRegion({x * cadr_step, 0, cadr});
      cadr -= 5000;
    }

    cv::imshow("stripe " + std::to_string(y), stripe.GetMat());
    film.InsertRegion(stripe);
  }

  cv::imshow("film", film.GetMat());

  bool res = false;

  if(SAVE_MODE){
    cv::FileStorage fs(R"(./../tests/resources/expected_film.xml)", cv::FileStorage::WRITE);
    fs << "expected_film" << film.GetMat();
    fs.release();
    wxLogMessage("Etalon saved");
  } else {
    cv::FileStorage fs(R"(./../tests/resources/expected_film.xml)", cv::FileStorage::READ);
    cv::Mat expected;
    fs["expected_film"] >> expected;
    fs.release();
    cv::imshow("expected", expected);

    res = !(cv::countNonZero(film.GetMat() != expected) == 0);
  }


  cv::waitKey();
  return res;
}
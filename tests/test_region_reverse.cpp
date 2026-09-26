#include <tape_scanner/scanner/Region.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

int main() {
  constexpr int cadr_cols = 200;
  constexpr int cadr_rows = 100;
  constexpr int cadr_overlap = 50;   // Нахлест кадров
  constexpr int stripe_overlap = 50; // Нахлест полос сканирования
  constexpr int cadr_step = cadr_cols - cadr_overlap;
  constexpr int stripe_step = cadr_rows - stripe_overlap;

  cv::Mat cadr(cadr_rows, cadr_cols, CV_16UC1, cv::Scalar(65000));

  Region film;
  for (int y = 4; y >=0; --y) {
    Region stripe(0, y * stripe_step, 0, 0);

    for (int x = 3; x >= 0; --x) {
      stripe.InsertRegion({x * cadr_step, 0, cadr});
      cadr -= 5000;
    }

    cv::imshow("stripe " + std::to_string(y), stripe.GetMat());
    film.InsertRegion(stripe);
  }

  cv::imshow("film", film.GetMat());

  auto test_file_name_path =
      R"(.\..\tests\resources\expected_film_reversed.xml)";

  // cv::FileStorage writer(test_file_name_path, cv::FileStorage::WRITE);
  // writer << "expected_film_reversed" << film.GetMat();
  // writer.release();

  cv::FileStorage fs(test_file_name_path,
                     cv::FileStorage::READ);
  cv::Mat expected;
  fs["expected_film_reversed"] >> expected;
  fs.release();
  cv::imshow("expected", expected);
  cv::waitKey();

  bool areEqual = cv::countNonZero(film.GetMat() != expected) == 0;
  return int(!areEqual);
}
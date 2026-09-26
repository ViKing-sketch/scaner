#include <wx/wx.h>
#include <tape_scanner/scanner/Film.hpp>
#include <opencv2/highgui.hpp>

int main() {
  wxLog::SetActiveTarget(new wxLogStream(&std::cout));
  wxLog::SetTimestamp("[%H:%M:%S,%l]");

  Film film;

  cv::Mat cadr1(200, 250, CV_16UC1, cv::Scalar(65000));

  for (int y = 0; y < 7; ++y) {
    for (int x = 0; x < 5; ++x) {
      film.PushRegion(cadr1, x * 200, y * 180);
      cadr1 -= 2000;
    }
  }

  cv::imshow("film", film.GetMat());
  cv::waitKey();

  return 0;
}
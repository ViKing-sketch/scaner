#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

int main() {

  auto mat =  cv::imread(
  R"(D:\Rasim\Add_job\simple_client_for_controller\build\snapshot.png)", CV_8UC1);

  // cv::FileStorage fs(
  //     R"(D:\Rasim\Add_job\simple_client_for_controller\build\calibr.xml)",
  //     cv::FileStorage::READ);
  // cv::Mat mat;
  // fs["mat"] >> mat;
  // fs.release();
  // mat.convertTo(mat, CV_8UC1, 100.0);

  cv::imshow("mat", mat);

  cv::waitKey();

  return 0;
}
#include <chrono>
#include <tape_scanner/scanner/Controller.hpp>
#include <tape_scanner/scanner/Scanner.hpp>
#include <wx/msw/app.h>
#include <wx/utils.h>
#include <wx/wx.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/photo.hpp>
#include <wx/log.h>

int main() {
  wxInitialize();
  if (!wxSocketBase::Initialize()) {
    wxLogError("Не удалось инициализировать сетевую подсистему!");
    return -1;
  }
  wxLog::SetTimestamp("[%Y-%m-%d %H:%M:%S,%l]");

  Camera camera;
  Controller controller("192.168.1.200", 4196);

  camera.TryConnect();

  if (!controller.TryConnect()) {
    std::cout << "Connection error!\n";
    return -1;
  }

  std::cout << "Successful connected\n";


  auto img_16_to_8 = [](cv::Mat img16) {
    cv::Mat img8;
    img16.convertTo(img8, CV_8UC1, 256 / 4096.0);
    /// DEGUG
    {
      // imshow("img8", img8);
      // cv::waitKey();
    }
    /// DEBUG
    return img8;
  };

  auto save_to = [](std::string key, cv::Mat img, std::string file_name = "test.xml") {
    cv::FileStorage fs(file_name, cv::FileStorage::APPEND);
    fs << key << img;
    fs.release();
    wxLogMessage("Write img %s to %s done", key, file_name);
  };

  auto read_mat_from = [](std::string key, std::string file_name = "test.xml") {
    cv::Mat img;
    cv::FileStorage fs(file_name, cv::FileStorage::READ);
    if (!fs.isOpened()) {
      wxLogError("Open %s for read failed", file_name);
      return img;
    }
    if (!fs[key].empty()) {
      fs[key] >> img;
      wxLogMessage("Read img %s done", key);
    } else {
      wxLogMessage("Key %s do not exist in %", key, file_name);
    }
    fs.release();

    return img;
  };

  auto get_dark = [&](double expose) {
    controller.LightEnable(false);
    auto img = camera.GetMat(expose);
    img.convertTo(img, CV_64FC1);
    controller.LightEnable(true);

    wxLogMessage("Dark for %g expose: %g", expose, cv::mean(img).val[0]);

    return img;
  };

  auto get_series_wit_expose = [&](float expose) {
    cv::Mat result = camera.GetCadrWithExposes({expose}).clone();

    int cnt = 5;

    for (int i = 0; i < cnt; ++i) {
      cv::Mat img = camera.GetCadrWithExposes({expose});
      result += img;
    }

    result /= (cnt + 1);
    return result;
  };


  get_dark(16);
  controller.LightEnable(false);
         controller.GoCameraToHome(true);
         controller.GoCameraTo(50000);
  controller.LightEnable(true);

  std::vector<cv::Mat> images;
  std::vector<float> exposes;
  for(int i = 16; i <= 30720; i*=2) exposes.push_back(i);
  cv::Point reper_pixel = {639, 126};

  auto get_reper_val_in_region = [&](const cv::Mat &img64) {
    int w = 5;
    int h = 5;
    double sum = 0;
    for (int y = 0; y < w; ++y) {
      for (int x = 0; x < h; ++x) {
        sum += img64.at<float>(reper_pixel.y + y, reper_pixel.x + x);
      }
    }

    auto res = sum / (w * h);

    wxLogMessage("calced_reper = %g", res);
    return res;
  };

  wxLogMessage("Start");


  // wxLogMessage("Reper val wit expose %g: %g", reper_expose, reper_val);

  int iter_count = 1;
  cv::Mat coefs = cv::Mat::zeros(exposes.size(), iter_count, CV_64FC1);
  auto tonemapper = cv::createTonemap();

  double prev_val = 0;
  for(int iter = 0; iter < iter_count; ++iter){
    int expose_num = 0;
    for (auto expose : exposes) {
      cv::Mat img32 = get_series_wit_expose(expose);
      auto cur_val = get_reper_val_in_region(img32);
      double ratio = cur_val / expose;
      double cur_div_prev = cur_val/prev_val;
      wxLogMessage("expose = %g, cur_val = %g, ratio = %g, cur_div_pve = %g", expose, cur_val, ratio, cur_div_prev);

      prev_val = cur_val;

      cv::cvtColor(img32, img32, cv::COLOR_GRAY2BGR);
      tonemapper->process(img32, img32);

      cv::Mat img8;
      img32.convertTo(img8, CV_8UC1, 255.0);
      cv::imshow("img8", img8);
      cv::waitKey();

      ++expose_num;
    }
  }

  wxLogMessage("Done");
  return 0;
}

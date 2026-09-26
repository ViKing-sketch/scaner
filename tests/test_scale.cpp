#include <chrono>
#include <tape_scanner/scanner/Controller.hpp>
#include <tape_scanner/scanner/Scanner.hpp>
#include <tape_scanner/scanner/Utils.hpp>
#include <wx/msw/app.h>
#include <wx/utils.h>
#include <wx/wx.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/photo.hpp>
#include <wx/log.h>

constexpr int MAX_PIX_VAL = 65534;

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

  // controller.GoCameraToHome(true);
  // controller.GoCameraTo(50000);
  controller.LightEnable(true);

  struct Cadr {
    Cadr(const cv::Mat &img, const std::string &name = "img") : name(name) {
      if (img.type() != CV_32FC1) {
        img.convertTo(this->img, CV_32FC1, 1.0);
      } else {
        this->img = img.clone();
      }

      calcs();
    }

    Cadr(const std::vector<cv::Mat> &imgs16, const std::string &name = "img") : name(name) {
      CV_Assert(imgs16.size());

      cv::Mat sum = cv::Mat::zeros(imgs16[0].size(), CV_64FC1);

      for (const auto &img : imgs16) {
        sum += img;
      }
      sum /= imgs16.size();
      sum.convertTo(this->img, CV_32FC1);
      calcs();
    }

    void calcs() {
      this->img += 1e-6;
      mean = cv::mean(this->img).val[0];
      cv::minMaxLoc(this->img, &min, &max);
    }

    cv::Mat img;
    std::string name;
    double mean;
    double min;
    double max;

    wxString toString() {
      return wxString::Format("img %ix%i, mean = %g, min = %g, max = %g", img.cols, img.rows, mean, min, max);
    }

    void SaveToDcm(std::string fname = "") {
      if (fname.empty()) {
        fname = name + ".dcm";
      }

      cv::Mat img16;
      img.convertTo(img16, CV_16UC1, 65536.0/max);
      saveToDcm(img16, fname);
    }

    void show(std::string win_name = "") {
      CV_Assert(img.type() == CV_32FC1);

      win_name = win_name.size() ? win_name : name;
      cv::Mat img8;
      img.convertTo(img8, CV_8U, max / 256.0);
      cv::imshow(win_name, img8);
      cv::waitKey(1);
    }

    void show_tonemap() {
      CV_Assert(img.type() == CV_32FC1);
      // auto tonemapper = cv::createTonemap();
      // cv::Mat img_color;
      // cv::cvtColor(img, img_color, cv::COLOR_GRAY2BGR);
      // tonemapper->process(img_color, img_color);

      cv::Mat img8;
      img.convertTo(img8, CV_8UC1, 256.0 / max);
      cv::imshow(name, img8);
    }
  };

  auto capture = [&](float expose) {
    std::vector<cv::Mat> imgs;
    for (int i = 0; i < 4; ++i) {
      imgs.push_back(camera.GetCadrWithExposes({expose})({100, 100, 1000, 700}));
    }

    auto cadr = Cadr(imgs, std::to_string(expose));
    wxLogMessage("Captured %s", cadr.toString());
    return cadr;
  };

  while (true) {
    float exp1 = 16 * 6;
    float exp2 = 16 * 10;

    wxLogMessage("\n --------------");

    auto img1 = capture(exp1);
    img1.show();

    auto img2 = capture(exp2);
    img2.show();

    // double scale_expose = exp1 / exp2;
    double scale = img1.mean / img2.mean;
    auto scaled_img1 = Cadr(img1.img.mul(1.0 / img2.img), "scaled_img1");
    wxLogMessage("%g/%g: scale = %g, scale_img info: %s", exp1, exp2, scale,
                 scaled_img1.toString());
    scaled_img1.show_tonemap();
    scaled_img1.SaveToDcm();

    exp2  = 16 * 22;
    auto img3 = capture(exp2);
    img3.show();

    auto scaled_img2 = Cadr(img1.img.mul(1.0 / img3.img), "scaled_img2");
    wxLogMessage("%g/%g: scale = %g, scale_img2 info: %s", exp1, exp2, img1.mean/img3.mean,
                 scaled_img2.toString());
    scaled_img2.show_tonemap();
    scaled_img2.SaveToDcm();

    auto diff = Cadr(scaled_img2.img.mul(1.0 / scaled_img1.img), "diff");
    wxLogMessage("diff info: %s", diff.toString());
    diff.show_tonemap();
    diff.SaveToDcm();

    if (cv::waitKey() == 27)
      break;
  }
  return 0;
}

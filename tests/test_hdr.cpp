#include <tape_scanner/scanner/Controller.hpp>
#include <tape_scanner/scanner/Scanner.hpp>
#include <chrono>
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

  controller.GoCameraToHome(true);
  controller.GoCameraTo(50000);
  controller.LightEnable(true);



  /// Калибровка
  camera.CalibrateResponses();


  // Мердж
  wxLogMessage("Start merge debevec");

  std::vector<float> exposes = {16, 80, 160, 800, 1600, 4000, 10000, 20000, 30000};

  while (true) {
    auto hdr_img = camera.GetCadrWithExposes(exposes);

    auto tonemapper = cv::createTonemap();
    cv::cvtColor(hdr_img, hdr_img, cv::COLOR_GRAY2BGR);
    tonemapper->process(hdr_img, hdr_img);

    hdr_img.convertTo(hdr_img, CV_8UC1, 255.0);

    cv::imshow("hdr_img", hdr_img);
    auto key = cv::waitKey();
    wxLogMessage("%i key pressed", key);

    if(key == 27)
      break;
  }
  return 0;
}

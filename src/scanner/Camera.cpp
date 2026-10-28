#include "tape_scanner/scanner/Utils.hpp"
#include <tape_scanner/scanner/Camera.hpp>
#include <tape_scanner/scanner/ExposurePulse.hpp>
#include <opencv2/photo.hpp>

namespace {

constexpr int BYTES_PEER_PIXEL = 2;
const std::string RESPONSES_FILE_NAME = "responses.xml";

void TryOrLogicError(CameraSdkStatus status, const std::string& error_msg) {
    if (status != CAMERA_STATUS_SUCCESS)
        throw std::runtime_error(error_msg);
}

} // namespace

Camera::Camera() {}

Camera::~Camera() {
  if (IsConnected())
    CameraUnInit(m_h_camera);

  if (this->m_frame_buffer)
    CameraAlignFree(this->m_frame_buffer);

  if (m_output_frame_buffer)
    delete[] m_output_frame_buffer;
}

bool Camera::TryConnect() {
  try {
    if (CameraSdkInit(0) != CAMERA_STATUS_SUCCESS) {
      throw std::logic_error("CameraSdkInit failed");
    }

    int h_camera = GetFirstCamera();
    InitResponses();
  } catch (std::runtime_error &e) {
    wxLogError(wxString::Format("Connect to camera error! %s", e.what()));
    return false;
  }

  wxLogMessage("Successful connect to camera");
  return true;
}

bool Camera::IsConnected() {
    return this->m_h_camera > -1;
}

void Camera::EnableAutoExpose(bool enable) {
    TryOrLogicError(CameraSetAeState(this->m_h_camera, enable),
                    "CameraSetAeState error!");
}

void Camera::SetExpose(int microseconds) {
    // Это затвор, не импульс подсветки. Скан передаёт сюда T + запас.
    TryOrLogicError(CameraSetExposureTime(m_h_camera, microseconds),
                    "SetExpose error!");
}

double Camera::GetExpose() {
    double expose = 0;
    TryOrLogicError(CameraGetExposureTime(m_h_camera, &expose),
                    "GetExpose error!");
    return expose;
}

uint8_t* Camera::GetCadr(tSdkFrameHead& frame_info, double expose) {
    uint8_t* raw_data = nullptr;

    if(expose != 0.0)
    SetExpose(expose);

    CameraClearBuffer(m_h_camera);
    CameraSoftTrigger(m_h_camera);

    TryOrLogicError(
        CameraGetImageBuffer(m_h_camera, &frame_info, &raw_data, 3000),
        "GetImageBufferPriority error!");


    // CameraImageProcess(this->m_h_camera, raw_data, this->m_frame_buffer,
    //                    &frame_info);

    auto unpack_raw_12 = [](const uint8_t* packed, uint16_t* unpacked, int width, int height) {
      int totalPixels = width * height;
      // Обрабатываем по 2 пикселя (3 байта) за итерацию
      for (int i = 0, j = 0; j < totalPixels; i += 3, j += 2) {
        uint8_t b0 = packed[i];    // Старший байт первого пикселя
        uint8_t b1 = packed[i+1];  // Младшие полубайты перого и второго пикселя
        uint8_t b2 = packed[i+2];  // Старший байт второго писеля

        uint16_t p0_std = (((b0  <<  4) &  0xFF0) | ((b1 >> 4) & 0xF )) & 0xFFF;
        uint16_t p1_std = (((b2  <<  4) &  0xFF0) | ((b1) & 0xF )) & 0xFFF;

        unpacked[j]   = p0_std;
        unpacked[j+1] = p1_std;
      }
    };

    unpack_raw_12(raw_data, (uint16_t*)this->m_frame_buffer, frame_info.iWidth, frame_info.iHeight);

    cv::Mat unpacked(frame_info.iHeight, frame_info.iWidth, CV_16UC1, this->m_frame_buffer);
    unpacked -= 18; // Dark
    unpacked *= (65536/3800.0);
    // memcpy(unpacked.data, this->m_frame_buffer, unpacked.total() * unpacked.elemSize());
    cv::imwrite("unpacked.png", unpacked);

    // memcpy(this->m_frame_buffer, raw_data, frame_info.iWidth * frame_info.iHeight * BYTES_PEER_PIXEL);
    CameraReleaseImageBuffer(this->m_h_camera, raw_data);

    return this->m_frame_buffer;
}

void Camera::SaveCadrTo(const std::string& dest_path) {
    tSdkFrameHead frame_info;
    auto* frame_buf = GetCadr(frame_info);

    TryOrLogicError(
        CameraSaveImage(this->m_h_camera, const_cast<char*>(dest_path.c_str()),
                        frame_buf, &frame_info,
                        this->m_is_mono ? FILE_BMP_8BIT : FILE_BMP, 100),
        "SaveImage error!");
}

wxBitmap Camera::Snapshot(double expose) {
    if (m_h_camera < 0) {
        throw std::logic_error("Camera not connected!");
    }

    auto mat = GetMat(expose);
    mat.convertTo(mat, CV_8UC1, 1/256.0);
    cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
    wxImage img(mat.cols, mat.rows,
                mat.data, true);
    return wxBitmap(img);
}

cv::Mat Camera::GetMat(double shutter_us, bool log_shutter) {
  std::lock_guard lock(m_mutex);

    if (m_h_camera < 0) {
        throw std::logic_error("Camera not connected!");
    }

    tSdkFrameHead frame_info;
    auto* frame_buf = reinterpret_cast<uint16_t*>(GetCadr(frame_info, shutter_us));

    cv::Mat res(frame_info.iHeight, frame_info.iWidth, CV_16UC1);
    memcpy(res.data, frame_buf, res.total() * res.elemSize());

    double min_val, max_val;
    cv::minMaxLoc(res, &min_val, &max_val);
    double mean_val = cv::mean(res).val[0];
    if (shutter_us != 0.0 && log_shutter) {
      wxLogMessage("Camera: GetMat(shutter = %g us). min = %g, mean = %g, max = %g", shutter_us, min_val, mean_val,
                   max_val);
    } else {
      wxLogMessage("Camera: GetMat(expose = %g). min = %g, mean = %g, max = %g", shutter_us, min_val, mean_val, max_val);
    }
    return res;
}

cv::Mat Camera::GetCadrWithExposes(std::vector<float> pulse_targets_us, IControllerPulse *light){
  std::vector<cv::Mat> imgs_16;
  std::vector<float> weights;
  imgs_16.reserve(pulse_targets_us.size());
  weights.reserve(pulse_targets_us.size());

  for (auto target_us : pulse_targets_us) {
    const ExposureShot shot = light != nullptr ? CommandExposure(*light, target_us) : PlanExposure(target_us);
    if (!shot.planned || !(shot.shutter_us >= shot.pulse_us)) {
      wxLogError("Exposure plan rejected for %g us", target_us);
      continue;
    }
    if (light != nullptr && !shot.pulse_commanded) {
      wxLogMessage("Exposure pulse %g us was not accepted (TODO Z04)", shot.pulse_us);
    }
    // Вес HDR — импульс. Камере уходит затвор, который этот импульс перекрывает.
    weights.push_back(static_cast<float>(shot.pulse_us));
    imgs_16.push_back(GetMat(shot.shutter_us, true));
  }

  if (imgs_16.empty()) {
    return {};
  }
  return ToRadiance(imgs_16, weights);
}

int Camera::GetFirstCamera() {
    constexpr int max_devs = 4;
    tSdkCameraDevInfo dev_list[max_devs];

    int devs_cnt = max_devs;
    TryOrLogicError(CameraEnumerateDevice(dev_list, &devs_cnt),
                    "CameraEnumerateDevice error!");

    if (devs_cnt < 1) {
        throw std::logic_error("Connected devices not found.");
    }

    TryOrLogicError(CameraInit(&dev_list[0], -1, -1, &this->m_h_camera),
                    "CameraInit error!");

    tSdkCameraCapbility camera_info;
    TryOrLogicError(CameraGetCapability(this->m_h_camera, &camera_info),
                    "CameraGetCapability error!");

    this->m_is_mono = camera_info.sIspCapacity.bMonoSensor;
    if (this->m_is_mono) {
        CameraSetIspOutFormat(this->m_h_camera, CAMERA_MEDIA_TYPE_MONO12);
        this->m_pixel_size = this->m_is_mono ? 2 : 3;
    }

    TryOrLogicError(CameraSetTriggerMode(this->m_h_camera, SOFT_TRIGGER),
                    "CameraSetTriggerMode error!");

    TryOrLogicError(CameraSetTriggerCount(this->m_h_camera, 1),
                    "CameraSetTriggerCount error!");

    EnableAutoExpose(false);

    TryOrLogicError(CameraPlay(this->m_h_camera), "CameraPlay error!");

    this->m_height = camera_info.sResolutionRange.iHeightMax;
    this->m_width = camera_info.sResolutionRange.iWidthMax;
    size_t buffer_size = this->m_height * this->m_width * this->m_pixel_size;
    this->m_frame_buffer = CameraAlignMalloc(buffer_size, 16);
    this->m_output_frame_buffer = new uint8_t[buffer_size * 3];

    return this->m_h_camera;
}

void Camera::CalibrateResponses()
{
  constexpr int MAX_PIX_TRESHOLD = 65534;
  constexpr int MIN_PIX_TRESHOLD = 5000;

  auto img_16_to_8 = [](const cv::Mat& img16){
    cv::Mat img8;
    img16.convertTo(img8, CV_8UC1, 256.0 / 65536.0);
    return img8;
  };

  wxLogMessage("Start capture cadres for calibrate");

  std::vector<cv::Mat> images;
  std::vector<float> exposes;
  cv::Mat response;
  cv::Rect roi = {100, 100, 800, 500};

  for (int i = 1; i < 1875; i++) {
    double expose = i * 16;
    // expose — цель импульса. Затвор длиннее на запас и перекрывает импульс.
    // Вес кривой — импульс. Команду импульса здесь не слать: контроллера нет,
    // кадр TCP — TODO(Z04).
    const ExposureShot shot = PlanExposure(expose);
    if (!shot.planned) {
      continue;
    }
    cv::Mat img16 = GetMat(shot.shutter_us, false)(roi);

    double min_val, max_val;
    {
      double mean = cv::mean(img16).val[0];
      cv::minMaxIdx(img16, &min_val, &max_val);

      wxLogMessage("min: %g, max: %g, mean: %g", min_val, max_val, mean);
    }

    if(max_val < MIN_PIX_TRESHOLD){
      wxLogMessage("max_val less than %i. Skiped cadr", MIN_PIX_TRESHOLD);
      continue;
    }

    if(max_val >= MAX_PIX_TRESHOLD){
      wxLogMessage("max_val greater %i. Stop capture.", MAX_PIX_TRESHOLD);
      break;
    }

    images.push_back(img_16_to_8(img16));
    exposes.push_back(static_cast<float>(shot.pulse_us));
  }

  wxLogMessage("Captured %zu cadres", images.size());

  wxLogMessage("Start calibrate debevec");
  cv::Ptr<cv::CalibrateDebevec> calibrator = cv::createCalibrateDebevec(500);
  calibrator->process(images, response, exposes);

  writeMatTo(response, "responses", RESPONSES_FILE_NAME);
  wxLogMessage("Calibrate done. Responses saved to %s", RESPONSES_FILE_NAME);
}

void Camera::InitResponses()
{
  auto response_to_16 = [](cv::Mat resp_8){
    // resp_8: 256x1, CV_32FC1
    CV_Assert(resp_8.rows == 256 && resp_8.cols == 1 && resp_8.type() == CV_32FC1);

    cv::Mat resp_16 = cv::Mat::zeros(65536, 1, CV_32FC1);
    const float* src = resp_8.ptr<float>(0);

    for (int i = 0; i < resp_16.rows; ++i) {
      int k = i / 256;          // индекс в исходном массиве
      float t = (i % 256) / 256.0f; // 0..1 внутри интервала

      float v;
      if (k < 255) {
        // линейная интерполяция между src[k] и src[k+1]
        v = (1.0f - t) * src[k] + t * src[k+1];
      } else {
        // последний блок: используем последнее значение (как в исходном коде)
        v = src[255];
      }
      resp_16.at<float>(i, 0) = v;
    }
    return resp_16;
  };

  auto responses_8 = readMatFromFile("responses", RESPONSES_FILE_NAME);
  if(!responses_8.empty()){
    m_responses = response_to_16(responses_8);
  }

}

cv::Mat Camera::ToRadiance(std::vector<cv::Mat> imgs_16, std::vector<float> exposes) {
  // exposes — длительности импульса подсветки, мкс. Debevec взвешивает по ним, не по затвору.
  cv::Mat img_32f;
  auto merger = cv::createMergeDebevec();
  if (!m_responses.empty()) {
    merger->process(imgs_16, img_32f, {exposes}, m_responses);
  } else {
    wxLogMessage("ToRadiance without responses!");
    merger->process(imgs_16, img_32f, {exposes});
  }

  return img_32f;
}



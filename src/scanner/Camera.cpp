#include "tape_scanner/scanner/Utils.hpp"
#include <tape_scanner/scanner/Camera.hpp>
#include <tape_scanner/scanner/RawFrame.hpp>
#include <opencv2/photo.hpp>

#include <cstring>

static_assert(tape_scanner::kMediaMono8 == CAMERA_MEDIA_TYPE_MONO8);
static_assert(tape_scanner::kMediaMono12 == CAMERA_MEDIA_TYPE_MONO12);
static_assert(tape_scanner::kMediaMono12Packed == CAMERA_MEDIA_TYPE_MONO12_PACKED);
static_assert(tape_scanner::kMediaMono16 == CAMERA_MEDIA_TYPE_MONO16);
static_assert(tape_scanner::kMediaMono12PackedMv == CAMERA_MEDIA_TYPE_MONO12_PACKED_MV);

namespace {

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
    m_significant_bits = 0;
    m_media_type = 0;
    uint8_t* raw_data = nullptr;

    if(expose != 0.0)
    SetExpose(expose);

    CameraClearBuffer(m_h_camera);
    CameraSoftTrigger(m_h_camera);

    TryOrLogicError(
        CameraGetImageBuffer(m_h_camera, &frame_info, &raw_data, 3000),
        "GetImageBufferPriority error!");

    struct ReleaseSdkBuffer {
        int handle;
        uint8_t* data;
        ~ReleaseSdkBuffer() {
            if (data != nullptr)
                CameraReleaseImageBuffer(handle, data);
        }
    } release{m_h_camera, raw_data};

    // Формат берётся из заголовка кадра. CameraSetIspOutFormat его не подтверждает.
    // CameraImageProcess не вызывается: это путь ISP, не RAW.
    tape_scanner::RawDecodeRequest request;
    request.media_type = frame_info.uiMediaType;
    request.width = frame_info.iWidth;
    request.height = frame_info.iHeight;
    request.stride = 0;
    request.byte_order = tape_scanner::RawByteOrder::LittleEndian;
    request.data = raw_data;
    request.size = frame_info.uBytes;

    const tape_scanner::RawDecodeResult decoded = tape_scanner::DecodeRawFrame(request);
    if (decoded.status != tape_scanner::RawDecodeStatus::Ok) {
        throw std::runtime_error(std::string("RAW decode failed: ") +
                                 tape_scanner::RawDecodeStatusText(decoded.status));
    }

    const size_t bytes = decoded.image.samples.size() * sizeof(uint16_t);
    const size_t capacity = m_height * m_width * m_pixel_size;
    if (m_frame_buffer == nullptr || m_pixel_size < sizeof(uint16_t) || bytes > capacity) {
        throw std::runtime_error("RAW frame does not fit the capture buffer");
    }
    std::memcpy(m_frame_buffer, decoded.image.samples.data(), bytes);
    m_significant_bits = decoded.image.significant_bits;
    m_media_type = decoded.image.media_type;
    return m_frame_buffer;
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

cv::Mat Camera::GetMat(double expose) {
  std::lock_guard lock(m_mutex);

    if (m_h_camera < 0) {
        throw std::logic_error("Camera not connected!");
    }

    tSdkFrameHead frame_info;
    auto* frame_buf = reinterpret_cast<uint16_t*>(GetCadr(frame_info, expose));

    cv::Mat res(frame_info.iHeight, frame_info.iWidth, CV_16UC1);
    memcpy(res.data, frame_buf, res.total() * res.elemSize());

    double min_val, max_val;
    cv::minMaxLoc(res, &min_val, &max_val);
    double mean_val = cv::mean(res).val[0];
    wxLogMessage("Camera: GetMat(expose = %g). min = %g, mean = %g, max = %g", expose, min_val, mean_val, max_val);
    return res;
}

cv::Mat Camera::GetCadrWithExposes(std::vector<float> exposes){
  std::vector<cv::Mat> imgs_16;

  for(auto expose : exposes) {
    imgs_16.push_back(GetMat(expose));
  }

  return ToRadiance(imgs_16, exposes);
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
    cv::Mat img16 = GetMat(expose)(roi);

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
    exposes.push_back(expose);
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



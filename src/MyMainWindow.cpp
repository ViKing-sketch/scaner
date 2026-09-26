#include "MyMainWindow.hpp"

// for compilers that support precompilation, includes "wx/wx.h"
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/msw/rcdefs.h>
#include <wx/valnum.h>

#include <winsock2.h>

#include <dcmtk/dcmdata/dcdicent.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdict.h"
#include "dcmtk/ofstd/ofstd.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmdata/dcdeftag.h"
#include "dcmtk/dcmdata/dcuid.h"

#include "tape_scanner/utils/ini.h"
#include "tape_scanner/utils/language.h"
#include "tape_scanner/utils/crc32.h"
#include "camera_api/CameraApi.h"
#include "tape_scanner/dicom_viewer/DicomProcessor.h"
#include "tape_scanner/utils/Logger.h"

#include <tape_scanner/dicom_generator/DicomGeneratorUtils.hpp>
#include <tape_scanner/dicom_generator/DcmCreator.hpp>
#include <tape_scanner/dicom_generator/BinToDcmConverter.hpp>
#include <tape_scanner/dicom_generator/JpegToDcmConverter.hpp>

#include <tape_scanner/scanner/Utils.hpp>

#include <Windows.h>
#include <string>
#include <cstring>
#include <fstream>
#include <vector>
#include <ctime>
#include <time.h>
#include <iostream>
#include <exception>

using namespace std;

// translation related defines
#ifdef _
#undef _
#endif
#define _(STR) lng->translate(STR)

#define DEF_FRAME_MAX_WIDTH UINT16_MAX
#define DEF_FRAME_MAX_HEIGHT UINT16_MAX
#define DEF_FRAME_MAX_COUNT UINT16_MAX

#define DEBUG 0

MyMainWindow::MyMainWindow(wxWindow* parent) : MainWindow(parent), m_dcm_processor(*m_gl_image_canvas) {
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data)) {
    wxMessageBox(_("Network setup failed."), _("Error"), wxOK);
    Close(true);
  }
  // CRC32 init
  crc32_init();
  // language converter setup
  lng = new Language();
  // setup status
  SetStatusText(_("Welcome"), 2);
  // settings get/set/create
  bool update = false;
  bool create = false;
  mINI::INIFile settings_file(settings_file_path);
  if (!settings_file.read(settings)) {
    settings["settings"]["language"] = "English";
    settings["controller"]["ip_address"] = controller_ip_address_default.c_str().AsChar();
    settings["controller"]["net_mask"] = controller_net_mask_default.c_str().AsChar();
    settings["controller"]["gateway"] = controller_gateway_default.c_str().AsChar();
    settings["controller"]["udp_port"] = controller_udp_port_default.c_str().AsChar();
    auto str = controller_init_cmds_default;
    str.Replace("\n","\\n");
    settings["controller"]["init_cmds"] = str.c_str().AsChar();
    settings["camera"]["ip_address"] = camera_ip_address_default.c_str().AsChar();
    settings["camera"]["net_mask"] = camera_net_mask_default.c_str().AsChar();
    settings["camera"]["gateway"] = camera_gateway_default.c_str().AsChar();
    settings["camera"]["dhcp"] = camera_dhcp_default ? "1" : "0";
    settings["camera"]["exposure_time"] = camera_exposure_time_default.c_str().AsChar();
    settings["scan"]["width"] = scan_width_default.c_str().AsChar();
    settings["scan"]["height"] = scan_height_default.c_str().AsChar();
    settings["scan"]["ppm"] = scan_ppm_default.c_str().AsChar();
    create = true;
  }
  if (!settings.has("settings") || !settings["settings"].has("language")) {
    settings["settings"]["language"] = "English";
    update = true;
  }
  wxString settings_language = wxString(settings["settings"]["language"]);
  if (settings_language != wxT("English")) {
    if (settings_language == wxT("Russian")) {
      lng->set(wxT("Russian"));
      lng->translate_frame(this, wxT("LAST"), wxT("CURRENT"), false);
    } else {
      settings["settings"]["language"] = "English";
      update = true;
    }
  }
  if (!settings.has("controller") || !settings["controller"].has("ip_address")) {
    settings["controller"]["ip_address"] = controller_ip_address_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("controller") || !settings["controller"].has("net_mask")) {
    settings["controller"]["net_mask"] = controller_net_mask_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("controller") || !settings["controller"].has("gateway")) {
    settings["controller"]["gateway"] = controller_gateway_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("controller") || !settings["controller"].has("udp_port")) {
    settings["controller"]["udp_port"] = controller_udp_port_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("controller") || !settings["controller"].has("init_cmds")) {
    auto str = controller_init_cmds_default;
    str.Replace("\n","\\n");
    settings["controller"]["init_cmds"] = str.c_str().AsChar();
    update = true;
  }
  if (!settings.has("camera") || !settings["camera"].has("ip_address")) {
    settings["camera"]["ip_address"] = camera_ip_address_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("camera") || !settings["camera"].has("net_mask")) {
    settings["camera"]["net_mask"] = camera_net_mask_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("camera") || !settings["camera"].has("gateway")) {
    settings["camera"]["gateway"] = camera_gateway_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("camera") || !settings["camera"].has("dhcp")) {
    settings["camera"]["dhcp"] = camera_dhcp_default ? "1" : "0";
    update = true;
  }
  if (!settings.has("camera") || !settings["camera"].has("exposure_time")) {
    settings["camera"]["exposure_time"] = camera_exposure_time_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("scan") || !settings["scan"].has("width")) {
    settings["scan"]["width"] = scan_width_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("scan") || !settings["scan"].has("height")) {
    settings["scan"]["height"] = scan_height_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("scan") || !settings["scan"].has("ppm")) {
    settings["scan"]["ppm"] = scan_ppm_default.c_str().AsChar();
    update = true;
  }
  m_controller_ip_address_text->SetValue(wxString(settings["controller"]["ip_address"]));
  m_controller_net_mask_text->SetValue(wxString(settings["controller"]["net_mask"]));
  m_controller_gateway_text->SetValue(wxString(settings["controller"]["gateway"]));
  m_controller_udp_port_text->SetValue(wxString(settings["controller"]["udp_port"]));
  auto str = wxString(settings["controller"]["init_cmds"]);
  str.Replace("\\n","\n");
  m_controller_init_cmds_text->SetValue(str);
  m_camera_ip_address_text->SetValue(wxString(settings["camera"]["ip_address"]));
  m_camera_net_mask_text->SetValue(wxString(settings["camera"]["net_mask"]));
  m_camera_gateway_text->SetValue(wxString(settings["camera"]["gateway"]));
  m_camera_dhcp_checkbox->SetValue(settings["camera"]["dhcp"] == "1" ? true : false);
  m_camera_exposure_time_text->SetValue(wxString(settings["camera"]["exposure_time"]));
  m_scan_width_text->SetValue(wxString(settings["scan"]["width"]));
  m_scan_length_text->SetValue(wxString(settings["scan"]["height"]));
  autosave_controller_ip_address = m_controller_ip_address_text->GetValue();
  autosave_controller_net_mask = m_controller_net_mask_text->GetValue();
  autosave_controller_gateway = m_controller_gateway_text->GetValue();
  autosave_controller_udp_port = m_controller_udp_port_text->GetValue();
  autosave_controller_init_cmds = m_controller_init_cmds_text->GetValue();
  autosave_camera_ip_address = m_camera_ip_address_text->GetValue();
  autosave_camera_net_mask = m_camera_net_mask_text->GetValue();
  autosave_camera_gateway = m_camera_gateway_text->GetValue();
  autosave_camera_dhcp = m_camera_dhcp_checkbox->GetValue();
  autosave_camera_exposure_time = m_camera_exposure_time_text->GetValue();
  autosave_scan_width = m_scan_width_text->GetValue();
  autosave_scan_height = m_scan_length_text->GetValue();
  if (create) _settings_file_create(true);
  if (update) _settings_file_update(true);
  // start timers
  m_service_timer.Start( 1000 );
  m_controller_timer.Start( 2000 );
  m_camera_timer.Start( 1000 );

  m_window_level_scroll_bar->SetRange(65535);
  m_window_level_scroll_bar->SetThumbPosition(500);

  m_window_width_scroll_bar->SetRange(65535);
  m_window_width_scroll_bar->SetThumbPosition(500);

  m_frame_window_level->SetValidator(wxTextValidator(wxFILTER_NUMERIC));
  m_frame_window_width->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

  m_current_frame->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

  SetupDcmGenerator();
}

MyMainWindow::~MyMainWindow() {
  delete lng;

  WSACleanup();
}

void MyMainWindow::OnServiceTimer( wxTimerEvent& event ) {
  // autosaving data
  int update = 0;
  if (autosave_controller_ip_address != m_controller_ip_address_text->GetValue()) {
    autosave_controller_ip_address = m_controller_ip_address_text->GetValue();
    settings["controller"]["ip_address"] = autosave_controller_ip_address.c_str();
    update++;
  }
  if (autosave_controller_net_mask != m_controller_net_mask_text->GetValue()) {
    autosave_controller_net_mask = m_controller_net_mask_text->GetValue();
    settings["controller"]["net_mask"] = autosave_controller_net_mask.c_str();
    update++;
  }
  if (autosave_controller_gateway != m_controller_gateway_text->GetValue()) {
    autosave_controller_gateway = m_controller_gateway_text->GetValue();
    settings["controller"]["gateway"] = autosave_controller_gateway.c_str();
    update++;
  }
  if (autosave_controller_udp_port != m_controller_udp_port_text->GetValue()) {
    autosave_controller_udp_port = m_controller_udp_port_text->GetValue();
    settings["controller"]["udp_port"] = autosave_controller_udp_port.c_str();
    update++;
  }
  if (autosave_controller_init_cmds != m_controller_init_cmds_text->GetValue()) {
    autosave_controller_init_cmds = m_controller_init_cmds_text->GetValue();
    auto str = autosave_controller_init_cmds;
    str.Replace("\n","\\n");
    settings["controller"]["init_cmds"] = str.c_str().AsChar();
    update++;
  }
  if (autosave_camera_ip_address != m_camera_ip_address_text->GetValue()) {
    autosave_camera_ip_address = m_camera_ip_address_text->GetValue();
    settings["camera"]["ip_address"] = autosave_camera_ip_address.c_str();
    update++;
  }
  if (autosave_camera_net_mask != m_camera_net_mask_text->GetValue()) {
    autosave_camera_net_mask = m_camera_net_mask_text->GetValue();
    settings["camera"]["net_mask"] = autosave_camera_net_mask.c_str();
    update++;
  }
  if (autosave_camera_gateway != m_camera_gateway_text->GetValue()) {
    autosave_camera_gateway = m_camera_gateway_text->GetValue();
    settings["camera"]["gateway"] = autosave_camera_gateway.c_str();
    update++;
  }
  if (autosave_camera_dhcp != m_camera_dhcp_checkbox->GetValue()) {
    autosave_camera_dhcp = m_camera_dhcp_checkbox->GetValue();
    settings["camera"]["dhcp"] = autosave_camera_dhcp ? "1" : "0";
    update++;
  }
  if (autosave_camera_exposure_time != m_camera_exposure_time_text->GetValue()) {
    autosave_camera_exposure_time = m_camera_exposure_time_text->GetValue();
    settings["camera"]["exposure_time"] = autosave_camera_exposure_time.c_str();
    update++;
  }
  if (autosave_scan_width != m_scan_width_text->GetValue()) {
    autosave_scan_width = m_scan_width_text->GetValue();
    settings["scan"]["width"] = autosave_scan_width.c_str();
    update++;
  }
  if (autosave_scan_height != m_scan_length_text->GetValue()) {
    autosave_scan_height = m_scan_length_text->GetValue();
    settings["scan"]["height"] = autosave_scan_height.c_str();
    update++;
  }
  if (update) _settings_file_update();
}

void MyMainWindow::OnControllerTimer( wxTimerEvent& event ) {
  InitController();
}

void MyMainWindow::OnCameraTimer( wxTimerEvent& event ) {
  if (tab == TAB_CAMERA) {
    if(!InitCamera()){
      return;
    }

    try {
      m_camera_bitmap->SetBitmap(m_camera->Snapshot(safeStrToDouble(m_camera_exposure_time_text->GetValue())));
    } catch (const std::exception &e) {
      wxLogError(e.what());
    }
  }

  return;

  static int reconnect_timer = 0;
  CameraSdkStatus status;
  double exposure_time;
  if (camera_id < 0) {
    // check/set reconnection timer
    reconnect_timer -= m_camera_timer.GetInterval();
    if (reconnect_timer > 0) return;
    reconnect_timer = 1000;
    SetStatusText(_("Connecting to camera.."), 1);
    // get cameras list
    int CameraNums = CameraEnumerateDeviceEx();
    if (CameraNums < 1) return;
    get_logger() << "Start init Camera \n";
    // init camera
    status = CameraInitEx(0, -1, -1, &camera_id);
    if (status != CAMERA_STATUS_SUCCESS) {
      camera_id = -1;
      get_logger() << "Error: start init Camera \n";
      return;
    }
    // load default parameters
    // TODO - check file existing
    get_logger() << "Start read Camera parameter from file \n";
    status = CameraReadParameterFromFile(camera_id, (char*)"camera_default_parameters.Config");
    if (status != CAMERA_STATUS_SUCCESS) {
      camera_id = -1;
      get_logger() << "Error: read camera parameter from file \n";
      return;
    }
    // get camera info
    CameraGetCapability(camera_id, &camera_info);
    // Switch camera mode to continuous acquisition
    CameraSetTriggerMode(camera_id, 0);
    // Manual exposure, set exposure time
    auto str = m_camera_exposure_time_text->GetValue();
    exposure_time = atof(str.c_str().AsChar());
    CameraSetAeState(camera_id, FALSE);
    CameraSetExposureTime(camera_id, exposure_time);
    // Let the SDK internal grab thread start working
    CameraPlay(camera_id);
    // allocate a buffer for camera frame
    camera_frame_buffer_size = camera_info.sResolutionRange.iWidthMax
                               * camera_info.sResolutionRange.iHeightMax
                               * 3;
    camera_frame_buffer = (BYTE*) CameraAlignMalloc(camera_frame_buffer_size, 16);
    // TODO - check buffer pointer
    // connection estabilished
    if (!camera_connected) {
      camera_connected = 1;
      SetStatusText(_("Camera connected"), 1);
    }
  }
  if (tab == TAB_CAMERA) {
    // set exposure time
    auto str = m_camera_exposure_time_text->GetValue();
    exposure_time = atof(str.c_str().AsChar());
    CameraSetExposureTime(camera_id, exposure_time);
    // Take a frame from the camera
    tSdkFrameHead FrameHead;
    BYTE* pRawData;
    status = CameraGetImageBuffer(camera_id, &FrameHead, &pRawData, 10);
    if (status == CAMERA_STATUS_SUCCESS) {
      CameraSetIspOutFormat(camera_id, CAMERA_MEDIA_TYPE_RGB8);
      CameraImageProcess(camera_id, pRawData, camera_frame_buffer, &FrameHead);
      CameraReleaseImageBuffer(camera_id, pRawData);
      CameraFlipFrameBuffer(camera_frame_buffer, &FrameHead, 1);
      // display a frame in the preview
      wxImage img(FrameHead.iWidth, FrameHead.iHeight, camera_frame_buffer, true);
      float ratio = ((float)FrameHead.iHeight) / ((float)FrameHead.iWidth);
      auto size = m_camera_bitmap->GetParent()->GetSize();
      //int box_width = size.GetWidth() - 20;
      int box_height = size.GetHeight() - 20;
      int scaled_width = (int) (((float)box_height) / ratio);
      int scaled_height = box_height - 20;
      auto scaled_img = img.Scale(scaled_width, scaled_height);
      wxBitmap bmp(scaled_img);
      m_camera_bitmap->SetBitmap(bmp);
    }
  } else {
    status = CameraConnectTest(camera_id);
  }
  if (status != CAMERA_STATUS_SUCCESS) {
    // update/check connection timeout
    camera_connection_timeout += m_camera_timer.GetInterval();
    if (camera_connection_timeout > camera_connection_timeout_max) {
      CameraUnInit(camera_id);
      CameraAlignFree(camera_frame_buffer);
      camera_id = -1;
      camera_frame_buffer = NULL;
      camera_frame_buffer_size = 0;
      if (camera_connected) {
        camera_connected = 0;
        SetStatusText(_("Camera disconnected"), 1);
      }
    }
  } else {
    camera_connection_timeout = 0;
  }
}

void MyMainWindow::OnExit(wxCommandEvent& event) {
  Close(true);
}

void MyMainWindow::OnAbout(wxCommandEvent& event) {
  wxMessageBox(_("Medical films scanner app."), _("About"), wxOK);
}

void MyMainWindow::OnLanguageEnglish(wxCommandEvent& event) {
  lng->set(wxT("English"));
  lng->translate_frame(this, wxT("LAST"), wxT("CURRENT"));
  settings["settings"]["language"] = "English";
  _settings_file_update();
}

void MyMainWindow::OnLanguageRussian(wxCommandEvent& event) {
  lng->set(wxT("Russian"));
  lng->translate_frame(this, wxT("LAST"), wxT("CURRENT"));
  settings["settings"]["language"] = "Russian";
  _settings_file_update();
}

void MyMainWindow::OnTabChange(wxNotebookEvent& event) {
  tab = m_tabs->FindPage( m_tabs->GetCurrentPage() );
}

void MyMainWindow::OnLightOnlyWhenCapture(wxCommandEvent &event)
{
  if(!InitScanner()) {
    return;
  }

  m_scanner->SetLightOnlyWhenCapture(m_checkBox_light_only_when_capture->GetValue());
}

void MyMainWindow::OnScanStart(wxCommandEvent &event) {
  if(!InitScanner()) {
    return;
  }

  double steps_by_peer_pix_x = safeStrToDouble(m_steps_by_per_pix_x_text->GetValue());
  double steps_by_peer_pix_y = safeStrToDouble(m_steps_by_per_pix_y_text->GetValue());
  m_scanner->SetStepsByPerPixel(steps_by_peer_pix_x, steps_by_peer_pix_y);

  m_scanner->StartScan(m_dst_scan_file_path->GetPath());
}

void MyMainWindow::OnScanStop( wxCommandEvent& event ) {
  m_scanner->StopScan();
}

void MyMainWindow::OnSaveFrameAsRGB8bitmap( wxCommandEvent& event ) {
  if (camera_id < 0) {
    SetStatusText(_("Failed to save a frame, camera not connected"), 2);
    return;
  }
  // Creates a "save file" dialog
  wxFileDialog* SaveDialog = new wxFileDialog(
      this, _("Save as.."), wxEmptyString, wxEmptyString,
      _("Bitmap files (*.bmp)|*.bmp"), wxFD_SAVE, wxDefaultPosition
      );
  // Shows a "save file" dialog
  if (SaveDialog->ShowModal() != wxID_OK) return;
  wxString file_path = SaveDialog->GetPath();
  SaveDialog->Destroy();
  // set exposure time
  auto str = m_camera_exposure_time_text->GetValue();
  double exposure_time = atof(str.c_str().AsChar());
  CameraSetExposureTime(camera_id, exposure_time);
  // Take a frame from the camera
  tSdkFrameHead FrameHead;
  BYTE* pRawData;
  CameraSdkStatus status = CameraGetImageBuffer(camera_id, &FrameHead, &pRawData, 3000);
  if (status == CAMERA_STATUS_SUCCESS) {
    CameraSetIspOutFormat(camera_id, CAMERA_MEDIA_TYPE_RGB8);
    CameraImageProcess(camera_id, pRawData, camera_frame_buffer, &FrameHead);
    CameraReleaseImageBuffer(camera_id, pRawData);
    status = CameraSaveImage(camera_id, (char*)file_path.c_str().AsChar(),
                             camera_frame_buffer, &FrameHead, FILE_BMP, 100);
    if (status == CAMERA_STATUS_SUCCESS) {
      SetStatusText(_("Camera frame successfuly saved"), 2);
    } else {
      SetStatusText(_("Failed to save camera frame into file"), 2);
    }
  } else {
    SetStatusText(_("Failed to get a frame from camera"), 2);
  }
}

void MyMainWindow::OnSaveFrameAsMonoRAW16( wxCommandEvent& event ) {
  if (camera_id < 0) {
    SetStatusText(_("Failed to save a frame, camera not connected"), 2);
    return;
  }
  // Creates a "save file" dialog
  wxFileDialog* SaveDialog = new wxFileDialog(
      this, _("Save as.."), wxEmptyString, wxEmptyString,
      _("RAW files (*.raw)|*.raw"), wxFD_SAVE, wxDefaultPosition
      );
  // Shows a "save file" dialog
  if (SaveDialog->ShowModal() != wxID_OK) return;
  wxString file_path = SaveDialog->GetPath();
  SaveDialog->Destroy();
  // set exposure time
  auto str = m_camera_exposure_time_text->GetValue();
  double exposure_time = atof(str.c_str().AsChar());
  CameraSetExposureTime(camera_id, exposure_time);
  // Take a frame from the camera
  tSdkFrameHead FrameHead;
  BYTE* pRawData;
  CameraSdkStatus status = CameraGetImageBuffer(camera_id, &FrameHead, &pRawData, 3000);
  if (status == CAMERA_STATUS_SUCCESS) {
    CameraSetIspOutFormat(camera_id, CAMERA_MEDIA_TYPE_MONO16);
    CameraImageProcess(camera_id, pRawData, camera_frame_buffer, &FrameHead);
    CameraReleaseImageBuffer(camera_id, pRawData);
    status = CameraSaveImage(camera_id, (char*)file_path.c_str().AsChar(),
                             camera_frame_buffer, &FrameHead, FILE_RAW_16BIT, 100);
    if (status == CAMERA_STATUS_SUCCESS) {
      SetStatusText(_("Camera frame successfuly saved"), 2);
    } else {
      SetStatusText(_("Failed to save camera frame into file"), 2);
    }
  } else {
    SetStatusText(_("Failed to get a frame from camera"), 2);
  }
}

void MyMainWindow::OnCameraSettingsSave( wxCommandEvent& event ) {
  if (camera_id < 0) {
    SetStatusText(_("Failed to send, camera not connected"), 2);
    return;
  }
  auto ip4_str = m_camera_ip_address_text->GetValue();
  auto gw4_str = m_camera_gateway_text->GetValue();
  auto nm4_str = m_camera_net_mask_text->GetValue();
  auto dhcp = m_camera_dhcp_checkbox->GetValue();
  CameraSdkStatus status;
  tSdkCameraDevInfo info;
  status = CameraGetEnumInfo(camera_id, &info);
  if (status != CAMERA_STATUS_SUCCESS) {
    SetStatusText(_("Failed to get camera device info"), 2);
    return;
  }
  CameraUnInit(camera_id);
  CameraAlignFree(camera_frame_buffer);
  camera_id = -1;
  camera_frame_buffer = NULL;
  camera_frame_buffer_size = 0;
  camera_connected = 0;
  SetStatusText(_("Camera disconnected"), 1);
  status = CameraGigeSetIp(&info,
                           ip4_str.c_str().AsChar(),
                           nm4_str.c_str().AsChar(),
                           gw4_str.c_str().AsChar(),
                           !dhcp
                           );
  if (status == CAMERA_STATUS_SUCCESS) {
    SetStatusText(_("New network settings were set for camera and the app"), 2);
  } else {
    SetStatusText(_("Failed to set new network settings for camera"), 2);
  }
}

void MyMainWindow::OnControllerSettingsSave( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get cmd text
  auto ip4_str = m_controller_ip_address_text->GetValue();
  auto gw4_str = m_controller_gateway_text->GetValue();
  auto nm4_str = m_controller_net_mask_text->GetValue();
  auto port_str = m_controller_udp_port_text->GetValue();
  auto cmd = wxString::Format(
      "settings set ip4:%s gw4:%s nm4:%s port:%s",
      ip4_str.Trim().Trim(false),
      gw4_str.Trim().Trim(false),
      nm4_str.Trim().Trim(false),
      port_str.Trim().Trim(false)
      );
  const char *cmd_char_str = cmd.c_str().AsChar();
  // send cmd
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnControllerInitBtn( wxCommandEvent& event ) {
  // Возможно не нужен
}

void MyMainWindow::OnControllerShellCmdInput( wxCommandEvent& event ) {
  if (!m_controller || !m_controller->IsConnected()) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get cmd text
  wxString cmd = m_controller_shell_cmd_text->GetValue();
  if (!cmd.length()) return;
  // send cmd
  m_controller->SendAnyCommand(cmd.ToStdString());
  {
    strcpy(controller_shell_last_cmd, cmd.ToStdString().c_str());
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
    m_controller_shell_cmd_text->SetValue(wxT(""));
  }
  // else {
  //   SetStatusText(_("Failed to send"), 2);
  // }
}

void MyMainWindow::OnControllerShellClear( wxCommandEvent& event ) {
  m_controller_shell_log->Clear();
  SetStatusText(_("Successfully cleared"), 2);
}

void MyMainWindow::OnOutput0Check( wxCommandEvent& event ) {
  if (!m_controller || !m_controller->IsConnected()) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get new value
  auto value = m_output0_check->GetValue();
  // send cmd
  m_controller->LightEnable(value);
}

void MyMainWindow::OnOutput1Check( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get new value
  auto value = m_output1_check->GetValue();
  // send cmd
  auto cmd = wxString::Format("gpio PE1 state %s", value ? "1" : "0");
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor0SetPos( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get new value
  auto value = m_motor0_pos_text->GetValue();
  double number;
  value.ToDouble(&number);
  // send cmd
  auto cmd = wxString::Format("stepdir 0 pos %f", number);
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
    m_motor0_goto_text->SetFocus();
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor0GoToPos( wxCommandEvent& event ) {
  if (!m_controller || !m_controller->IsConnected()) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get new value
  auto value = m_motor0_goto_text->GetValue();
  int pos;
  value.ToInt(&pos);

  try {
    m_controller->GoCameraTo(pos);
    SetStatusText(_("Successfully sent"), 2);
  } catch (...) {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor0DownBtnClick( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // send cmd
  wxString cmd;
  if (m_motor0_down_tbtn->GetValue()) {
    cmd = "stepdir 0 goto -9999.0";
  } else {
    cmd = "stepdir 0 stop";
  }
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor0UpBtnClick( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // send cmd
  wxString cmd;
  if (m_motor0_up_tbtn->GetValue()) {
    cmd = "stepdir 0 goto 9999.0";
  } else {
    cmd = "stepdir 0 stop";
  }
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor0Home( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // send cmd
  wxString cmd = "stepdir 0 goto 0.0";
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor0Stop( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // reset -/+ buttons state
  m_motor0_down_tbtn->SetValue(false);
  m_motor0_up_tbtn->SetValue(false);
  // send cmd
  wxString cmd = "stepdir 0 stop";
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor1SetPos( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get new value
  auto value = m_motor1_pos_text->GetValue();
  double number;
  value.ToDouble(&number);
  // send cmd
  auto cmd = wxString::Format("stepdir 1 pos %f", number);
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
    m_motor1_goto_text->SetFocus();
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor1GoToPos( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // get new value
  auto value = m_motor1_goto_text->GetValue();
  double number;
  value.ToDouble(&number);
  // send cmd
  auto cmd = wxString::Format("stepdir 1 goto %f", number);
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor1DownBtnClick( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // send cmd
  wxString cmd;
  if (m_motor1_down_tbtn->GetValue()) {
    cmd = "stepdir 1 goto -9999.0";
  } else {
    cmd = "stepdir 1 stop";
  }
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor1UpBtnClick( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // send cmd
  wxString cmd;
  if (m_motor1_up_tbtn->GetValue()) {
    cmd = "stepdir 1 goto 9999.0";
  } else {
    cmd = "stepdir 1 stop";
  }
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor1Home( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // send cmd
  wxString cmd = "stepdir 1 goto 0.0";
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnMotor1Stop( wxCommandEvent& event ) {
  if (controller_socket == INVALID_SOCKET) {
    SetStatusText(_("Failed to send, controller not connected"), 2);
    return;
  }
  // reset -/+ buttons state
  m_motor1_down_tbtn->SetValue(false);
  m_motor1_up_tbtn->SetValue(false);
  // send cmd
  wxString cmd = "stepdir 1 stop";
  const char *cmd_char_str = cmd.c_str().AsChar();
  if (_sendto_controller(cmd_char_str, cmd.length()) >= 0) {
    strcpy(controller_shell_last_cmd, cmd_char_str);
    _controller_shell_log(wxT("TX: ") + cmd + wxT("\n"));
    SetStatusText(_("Successfully sent"), 2);
  } else {
    SetStatusText(_("Failed to send"), 2);
  }
}

void MyMainWindow::OnCalibrateCamera(wxCommandEvent &event)
{
  if(!InitScanner()) {
    return;
  }

  m_scanner->CalibrateScanner();
}

void MyMainWindow::OnCalculateOffset(wxCommandEvent &event)
{
  if(!m_controller || !m_controller->IsConnected()){
    wxLogError("Contolles is not connected!");
    return;
  }

  m_scanner->AutoCalibrateCamera();
}

void MyMainWindow::OnDCMFileOpen(wxCommandEvent& event)
{
  // Creates an "open file" dialog
  wxFileDialog* OpenDialog = new wxFileDialog(
      this, _("Open DCM file.."), wxEmptyString, wxEmptyString,
      _("DICOM files (*.dcm)|*.dcm"), wxFD_OPEN, wxDefaultPosition
      );
  do {
    // Shows an "open file" dialog
    if (OpenDialog->ShowModal() != wxID_OK) break;
    _log(wxT("\n") + _("--- DICOM FILE OPEN ---") + wxT("\n") + get_time_str());
    open_file_name = OpenDialog->GetFilename();
    open_file_path = OpenDialog->GetPath();
    OpenDialog->Destroy();
    // show file path
    _log(wxString::Format(_("File name: %s\n"), open_file_name));
    _log(wxString::Format(_("File path: %s\n"), open_file_path));
    // load file
    // m_gl_image_canvas->SetImage(open_file_path);
    try
    {
      m_dcm_processor.Open(open_file_path);
      open_file_frames_cnt = m_dcm_processor.GetFramesCount();
      open_file_frame_width = m_dcm_processor.GetFrameWidth();
      open_file_frame_height = m_dcm_processor.GetFrameHeight();
    }
    catch(std::exception& ex)
    {
      _log(wxString::Format(_("%s\n"), ex.what()));
      break;
    }


    DcmFileFormat file;
    OFCondition status = file.loadFile((const OFFilename)open_file_path.mb_str());
    if (status.bad()) {
      _log(_("ERROR: Can't open file for reading.\n"));
      break;
    }
    // setup opened file info/data/controls
    open_file_tmp_path = open_file_path + ".bmp";
    open_file_frame = 1;
    open_file_window_level = 500;
    open_file_window_width = 500;
    open_file_rotation = 0;
    open_file_flip = 0;
    m_current_frame->SetValue(wxString::Format("%i", m_dcm_processor.GetCurFrameNum() + 1));
    m_frame_window_level->SetValue(wxString::Format("%i",open_file_window_level));
    m_frame_window_width->SetValue(wxString::Format("%i",open_file_window_width));
    m_window_level_scroll_bar->SetThumbPosition(open_file_window_level);
    m_window_width_scroll_bar->SetThumbPosition(open_file_window_width);
    m_rotation0_btn->SetValue(true);
    m_rotation90Left_btn->SetValue(false);
    m_rotation90Right_btn->SetValue(false);
    m_rotation180_btn->SetValue(false);
    m_flipH_btn->SetValue(false);
    m_flipV_btn->SetValue(false);
    // display file tags data
    DcmDataset* dataset = file.getDataset();
    OFString str;
    wxVector<wxVariant> row;
    DcmDataDictionary* dcm_dict = new DcmDataDictionary(OFTrue, OFFalse);
    DcmTagKey* tag;
    m_tags_view->DeleteAllItems();
    for (auto it = dcm_dict->normalBegin(); it != dcm_dict->normalEnd(); it++) {
      tag = (DcmTagKey*)(*it);
      // ignore pixel data tag
      if (tag->getGroup() == 0x7FE0 && tag->getElement() == 0x0010) continue;
      if (dataset->findAndGetOFStringArray((*tag), str).good()) {
        row.clear();
        row.push_back(wxVariant(tag->toString().c_str()));
        row.push_back(wxVariant(dcm_dict->findEntry((*tag), nullptr)->getTagName()));
        row.push_back(wxVariant(str.c_str()));
        //m_tags_view->AppendItem(row);
        m_tags_view->InsertItem(0, row);
      }
    }
    delete dcm_dict;
    _log(_("Successfuly opened. See Data tab.\n"));
    // title setup
    SetTitle(wxString::Format(_("%s (frames: %i, size: %ix%i)"),
                              open_file_name, open_file_frames_cnt,
                              open_file_frame_width, open_file_frame_height));

  } while (0);
}

void MyMainWindow::OnCurrentFrameChange( wxCommandEvent& event ) {
  m_current_frame->GetValue().ToInt(&open_file_frame);
  open_file_frame = m_dcm_processor.SetFrame(open_file_frame - 1);
  m_current_frame->SetValue(wxString::Format(wxT("%i"), open_file_frame + 1));
}

void MyMainWindow::OnCurrentFramePrev( wxCommandEvent& event ) {
  open_file_frame = m_dcm_processor.PrevFrame();
  m_current_frame->SetValue(wxString::Format(wxT("%i"), open_file_frame + 1));
}

void MyMainWindow::OnCurrentFrameNext( wxCommandEvent& event ) {
  open_file_frame = m_dcm_processor.NextFrame();
  m_current_frame->SetValue(wxString::Format(wxT("%i"), open_file_frame + 1));
}

void MyMainWindow::OnWindowLevelChange( wxCommandEvent& event ) {
  int level = 500;
  m_frame_window_level->GetValue().ToInt(&level);
  m_window_level_scroll_bar->SetThumbPosition(level);

  const float range = m_window_level_scroll_bar->GetRange();
  m_dcm_processor.SetBright(1.0f - (range - level) / range);
}

void MyMainWindow::OnWindowLevelScroll(wxScrollEvent& win_level_data){
  const float range = m_window_level_scroll_bar->GetRange();
  m_dcm_processor.SetBright(1.0f - (range - win_level_data.GetInt()) / range);
  m_frame_window_level->SetValue(wxString::Format(wxT("%i"), win_level_data.GetInt()));
}

void MyMainWindow::OnWindowWidthChange( wxCommandEvent& event ) {
  int width = 500;
  m_frame_window_width->GetValue().ToInt(&width);
  m_window_width_scroll_bar->SetThumbPosition(width);

  const float range = m_window_width_scroll_bar->GetRange();
  m_dcm_processor.SetContrast(1.0f - (range - width) / range);
}

void MyMainWindow::OnWindowWidthScroll(wxScrollEvent& win_width_data){
  const float range = m_window_width_scroll_bar->GetRange();
  m_dcm_processor.SetContrast(1.0f - (range - win_width_data.GetInt()) / range);
  m_frame_window_width->SetValue(wxString::Format(wxT("%i"), win_width_data.GetInt()));
}

void MyMainWindow::OnFrameRotate0( wxCommandEvent& event ) {
  open_file_rotation = 0;
  m_rotation0_btn->SetValue(true);
  m_rotation90Left_btn->SetValue(false);
  m_rotation90Right_btn->SetValue(false);
  m_rotation180_btn->SetValue(false);
}

void MyMainWindow::OnFrameRotate90Left( wxCommandEvent& event ) {
  open_file_rotation = -90;
  m_rotation0_btn->SetValue(false);
  m_rotation90Left_btn->SetValue(true);
  m_rotation90Right_btn->SetValue(false);
  m_rotation180_btn->SetValue(false);
}

void MyMainWindow::OnFrameRotate90Right( wxCommandEvent& event ) {
  open_file_rotation = 90;
  m_rotation0_btn->SetValue(false);
  m_rotation90Left_btn->SetValue(false);
  m_rotation90Right_btn->SetValue(true);
  m_rotation180_btn->SetValue(false);
}

void MyMainWindow::OnFrameRotate180( wxCommandEvent& event ) {
  open_file_rotation = 180;
  m_rotation0_btn->SetValue(false);
  m_rotation90Left_btn->SetValue(false);
  m_rotation90Right_btn->SetValue(false);
  m_rotation180_btn->SetValue(true);
}

void MyMainWindow::OnFrameFlipHorizontally( wxCommandEvent& event ) {
  if (!m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 0;}
  if (m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 1;}
  if (!m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = 2;}
  if (m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = -1;}
}

void MyMainWindow::OnFrameFlipVertically( wxCommandEvent& event ) {
  if (!m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 0;}
  if (m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 1;}
  if (!m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = 2;}
  if (m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = -1;}
}

void MyMainWindow::SetupDcmGenerator(){
  // Ограничиваем параметры изображения (длина, ширина, кол-во фреймов) для ввода во вкладке генератора. В полях
  // можно вводить только цифры только в определенном диапазоне.
  {
    namespace dg = tape_scanner::dicom_generator;
    for (auto &[max_val, /*for*/ fields] :
         {std::pair{dg::FRAME_MAX_WIDTH, std::array{m_BINWidth,  m_DCMGenWidth}},
          {          dg::FRAME_MAX_HEIGHT,         {m_BINHeight, m_DCMGenHeight}},
          {          dg::FRAME_MAX_COUNT,          {m_BINFrames, m_DCMGenFrames}}})
    {
      for (auto fld : fields)
      {
        fld->SetValidator(wxIntegerValidator<size_t>(nullptr, 1, max_val));
        fld->SetMaxLength(std::to_string(max_val).length());
      }
    }
  }
}

bool MyMainWindow::InitController()
{
  SetStatusText(_("Connecting controller..."), 0);

  if(!m_controller){
    m_controller = std::make_shared<Controller>(m_controller_ip_address_text->GetValue(),
                                                safeStrToInt(m_controller_udp_port_text->GetValue()));
  }

  if (!m_controller->IsConnected()) {
    m_controller->TryConnect();
  }

  if (m_controller->IsConnected()) {
    SetStatusText(_("Controller connected"), 0);
    return true;
  }

  return false;
}

bool MyMainWindow::InitCamera()
{
  SetStatusText(_("Connecting to camera.."), 1);

  if (!m_camera) {
    m_camera = std::make_shared<Camera>();
  }

  if(!m_camera->IsConnected()){
    // SetStatusText(_("Camera disconnected"), 1);
    m_camera->TryConnect();
  }

  if( m_camera->IsConnected())
  {
    SetStatusText(_("Camera connected"), 1);
    return true;
  }

  return false;
}

bool MyMainWindow::InitScanner()
{
  if (m_scanner) {
    return true;
  }

  if (!InitCamera()) {
    wxMessageBox( _("Camera is not initialized!"), _("Error"), wxICON_ERROR);
    return false;
  }

  if (!InitController()) {
    wxMessageBox( _("Controller is not initialized!"), _("Error"), wxICON_ERROR);
    return false;
  }

  m_scanner = std::make_unique<Scanner>(*m_controller, *m_camera);

  return true;
}

void MyMainWindow::OnJPGToDCMConvert( wxCommandEvent& event ) {
  _log(wxT("\n") + _("--- JPEG TO DICOM ---") + wxT("\n") + get_time_str());
  wxString m_in_jpeg_file_path = m_JPGFilePicker->GetTextCtrlValue();
  _log(wxString::Format(_("File path: %s\n"), m_in_jpeg_file_path));

  try
  {
    auto result_dcm_file_path = tape_scanner::dicom_generator::ConvertJpegToDcm(m_in_jpeg_file_path);
    _log(wxString::Format(_("Successfuly converted. See - %s\n"), result_dcm_file_path));
  }
  catch( std::runtime_error& error)
  {
    _log(_(error.what()));
  }
  catch(...)
  {
    _log(_("ERROR: unexpected error occured when convert jpeg to dicom"));
  }
}

void MyMainWindow::OnBINToDCMConvert( wxCommandEvent& event ) {
  _log(wxT("\n") + _("--- BIN/RAW TO DICOM ---") + wxT("\n") + get_time_str());
  // get input file path
  wxString file_path = m_BINFilePicker->GetTextCtrlValue();
  _log(wxString::Format(_("Input file path: %s\n"), file_path));

         // get output image width
  size_t width = 0;
  if (!m_BINWidth->GetValue().ToULongLong(&width)) {width = 512;}
  else if (!width) {width = 1;}
  else if (width > DEF_FRAME_MAX_WIDTH) {width = DEF_FRAME_MAX_WIDTH;}
  m_BINWidth->SetValue(wxString::Format(wxT("%zu"), width));
  _log(wxString::Format(_("Output images width: %zu px\n"), width));

         // get output image height
  size_t height = 0;
  if (!m_BINHeight->GetValue().ToULongLong(&height)) {height = 512;}
  else if (!height) {height = 1;}
  else if (height > DEF_FRAME_MAX_HEIGHT) {height = DEF_FRAME_MAX_HEIGHT;}
  m_BINHeight->SetValue(wxString::Format(wxT("%zu"), height));
  _log(wxString::Format(_("Output images height: %zu px\n"), height));

         // get output image bits per pixel
  size_t bits = 8;
  switch (m_BINBits->GetSelection()) {
  case 0: bits = 8; break;
  case 1: bits = 16;
  }
  _log(wxString::Format(_("Output images color depth: %zu bits\n"), bits));

         // get output image frames count
  size_t frames = 0;
  if (!m_BINFrames->GetValue().ToULongLong(&frames)) {frames = 1;}
  else if (!frames) {frames = 1;}
  else if (frames > DEF_FRAME_MAX_COUNT) {frames = DEF_FRAME_MAX_COUNT;}
  m_BINFrames->SetValue(wxString::Format(wxT("%zu"), frames));
  _log(wxString::Format(_("Output file frames count: %zu\n"), frames));

         // get frames copy flag
  bool all_frames_as_1st = m_BINAllFramesAs1stFrame->GetValue();
  _log(wxString::Format(_("All frames as 1st frame: %s\n"), all_frames_as_1st ? _("YES") : _("NO")));

  try{
    using namespace tape_scanner::dicom_generator;
    ConvertBinToDcm(file_path, width, height, bits, frames, all_frames_as_1st,
                    [this](const std::string& msg){ _log(msg); });
  }
  catch(...){
    //TODO
  }
  // calculate minimum input file size
  size_t frame_bin_size = width * height * (bits/8);
  size_t src_file_size_min = frame_bin_size * frames;
  _log(wxString::Format(_("Input file minimum size: %zu bytes\n"), src_file_size_min));
  // setup desitnation file data
  wxString dest_file_path = file_path + ".dcm";
  // vars allocation
  char cstr[256];
  E_TransferSyntax writeXfer = EXS_LittleEndianImplicit;
  DcmFileFormat dcm_file;
  DcmDataset* dataset = dcm_file.getDataset();
  DcmMetaInfo* metainfo = dcm_file.getMetaInfo();
  OFCondition condition;
  ifstream in;
  size_t frame;
  bool stop;
  size_t src_file_size;
  Uint8 *buf = NULL;
  do {
    // open input file
    in.open(string(file_path.mb_str()), ios::in | ios::binary);
    if (!in.is_open()) {
      _log(_("ERROR: Can't open input file for reading.\n"));
      break;
    }
    // get input file size
    in.seekg(0,ios_base::end);
    src_file_size = in.tellg();
    in.seekg(0);
    _log(wxString::Format(_("Input file size: %zu bytes\n"), src_file_size));
    if (src_file_size < src_file_size_min) {
      _log(_("ERROR: Input file size is too small.\n"));
      break;
    }
    // allocate input buffer
    buf = new Uint8[src_file_size_min];
    if (!buf) {
      _log(_("ERROR: Failed to allocate RAM buffer.\n"));
      break;
    }
    // setup general info
    // see info here - https://dicom.innolitics.com/ciods/
    metainfo->putAndInsertOFStringArray(DCM_TransferSyntaxUID, UID_LittleEndianImplicitTransferSyntax);
    // setup general tags
    dataset->putAndInsertOFStringArray(DCM_SeriesNumber, "1");
    dataset->putAndInsertOFStringArray(DCM_SeriesDescription, "Converted (BIN/RAW to DICOM)");
    dataset->putAndInsertOFStringArray(DCM_SeriesInstanceUID, dcmGenerateUniqueIdentifier(cstr, SITE_SERIES_UID_ROOT));
    dataset->putAndInsertOFStringArray(DCM_Modality, "XC"); // External-camera Photography
    dataset->putAndInsertOFStringArray(DCM_ConversionType, "SI"); // Scanned Image
    dataset->putAndInsertOFStringArray(DCM_PatientName, "BIN^TO^DICOM");
    dataset->putAndInsertOFStringArray(DCM_SOPInstanceUID, dcmGenerateUniqueIdentifier(cstr, SITE_INSTANCE_UID_ROOT));
    dataset->putAndInsertOFStringArray(DCM_StudyID, dcmGenerateUniqueIdentifier(cstr, SITE_STUDY_UID_ROOT));
    dataset->putAndInsertOFStringArray(DCM_BurnedInAnnotation, "NO");
    dataset->putAndInsertOFStringArray(DCM_NominalScannedPixelSpacing, "1.0\\1.0"); // mm, row\column
    dataset->putAndInsertOFStringArray(DCM_PixelSpacing, "1.0\\1.0"); // mm, row\column
    dataset->putAndInsertOFStringArray(DCM_PixelAspectRatio, "[1, 1]"); // [row, column]
    dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation, "MONOCHROME2");
    dataset->putAndInsertOFStringArray(DCM_RescaleIntercept, "0");
    dataset->putAndInsertOFStringArray(DCM_RescaleSlope, "1");
    dataset->putAndInsertOFStringArray(DCM_RescaleType, "US");
    dataset->putAndInsertOFStringArray(DCM_PresentationLUTShape, "IDENTITY");
    dataset->putAndInsertUint16(DCM_SamplesPerPixel, 1);
    dataset->putAndInsertUint16(DCM_BitsAllocated, bits > 8 ? 16 : 8);
    dataset->putAndInsertUint16(DCM_BitsStored, bits);
    dataset->putAndInsertUint16(DCM_HighBit, bits-1);
    dataset->putAndInsertUint16(DCM_PixelRepresentation, 0);
    dataset->putAndInsertUint16(DCM_Rows, height);
    dataset->putAndInsertUint16(DCM_Columns, width);
    // setup multi-frame DICOM file
    if (frames > 1) {
      // setup storage
      dataset->putAndInsertOFStringArray(DCM_SOPClassUID, (bits > 8) ?
                                                              UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage :
                                                              UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage);
      // setup frames
      OFStandard::snprintf(cstr, 30, "%zu", frames);
      dataset->putAndInsertOFStringArray(DCM_NumberOfFrames, cstr);
      dataset->putAndInsertOFStringArray(DCM_FrameTime, "1000");
      dataset->putAndInsertTagKey(DCM_FrameIncrementPointer, DCM_FrameTime);
      // if all frames must be a copy of 1st frame
      if (all_frames_as_1st) {
        for (stop = false, frame = 0; frame < frames; frame++) {
          // read input file into buffer
          in.seekg(0);
          if (in.read((char *)&buf[frame_bin_size * frame], frame_bin_size).bad()) {
            _log(_("ERROR: Failed to read input file into RAM buffer.\n"));
            stop = true;
            break;
          }
        }
        if (stop) break;
        // all frames must be unique
      } else {
        // read whole input file into buffer
        if (in.read((char *)buf, src_file_size_min).bad()) {
          _log(_("ERROR: Failed to read whole input file into RAM buffer.\n"));
          break;
        }
      }
      // setup single-frame DICOM file
    } else {
      // setup storage
      dataset->putAndInsertString(DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
      // read whole input file into buffer
      if (in.read((char *)buf, src_file_size_min).bad()) {
        _log(_("ERROR: Failed to read whole input file into RAM buffer.\n"));
        break;
      }
    }
    // insert frames binary data
    dataset->putAndInsertUint8Array(DCM_PixelData, buf, src_file_size_min);
    // save destination file
    condition = dcm_file.saveFile((const OFFilename)dest_file_path.mb_str(),
                                  writeXfer, EET_ExplicitLength, EGL_recalcGL, EPD_noChange,
                                  OFstatic_cast(Uint32, 0), OFstatic_cast(Uint32, 0), EWM_fileformat);
    if (condition.bad()) {
      _log(_("ERROR: Failed to save resulting DICOM file.\n"));
      break;
    }
  } while (0);
  // free buffers
  if (buf) delete [] buf;
  if (in.is_open()) in.close();
  // show success message
  _log(wxString::Format(_("Successfuly converted. See - %s\n"), dest_file_path));
}

void MyMainWindow::OnDCMCreate( wxCommandEvent& event ) {
  _log(wxT("\n") + _("--- PARAMETRIC DICOM GENERATOR ---") + wxT("\n") + get_time_str());

         // get output file path
  wxString file_path = m_DCMSaveFilePicker->GetTextCtrlValue();
  _log(wxString::Format(_("Output file path: %s\n"), file_path));

         // get output image width
  size_t width = 0;
  if (!m_DCMGenWidth->GetValue().ToULongLong(&width)) {width = 512;}
  else if (!width) {width = 1;}
  else if (width > DEF_FRAME_MAX_WIDTH) {width = DEF_FRAME_MAX_WIDTH;}
  m_DCMGenWidth->SetValue(wxString::Format(wxT("%zu"), width));
  _log(wxString::Format(_("Output images width: %zu px\n"), width));

         // get output image height
  size_t height = 0;
  if (!m_DCMGenHeight->GetValue().ToULongLong(&height)) {height = 512;}
  else if (!height) {height = 1;}
  else if (height > DEF_FRAME_MAX_HEIGHT) {height = DEF_FRAME_MAX_HEIGHT;}
  m_DCMGenHeight->SetValue(wxString::Format(wxT("%zu"), height));
  _log(wxString::Format(_("Output images height: %zu px\n"), height));

         // get output image bits per pixel
  size_t bits = 8;
  switch (m_DCMGenBits->GetSelection()){
  case 0: bits = 8; break;
  case 1: bits = 16;
  }
  _log(wxString::Format(_("Output images color depth: %zu bits\n"), bits));

         // get output image frames count
  size_t frames = 0;
  if (!m_DCMGenFrames->GetValue().ToULongLong(&frames)) {frames = 1;}
  else if (!frames) {frames = 1;}
  else if (frames > DEF_FRAME_MAX_COUNT) {frames = DEF_FRAME_MAX_COUNT;}
  m_DCMGenFrames->SetValue(wxString::Format(wxT("%zu"), frames));
  _log(wxString::Format(_("Output file frames count: %zu\n"), frames));

         // get fill byte/word value
  size_t fill_value = 0;
  size_t fill_value_max = (1UL << bits) - 1;
  wxString fill_value_str = m_DCMGenFillValue->GetValue();
  if (fill_value_str.empty() || !fill_value_str.IsNumber() || !fill_value_str.ToULongLong(&fill_value) ) {
    fill_value = UINT32_MAX;
  } else if (fill_value > fill_value_max) {
    fill_value = fill_value_max;
    m_DCMGenFillValue->SetValue(wxString::Format(wxT("%zu"), fill_value));
  }
  if (fill_value == UINT32_MAX)
    _log(wxString::Format(_("Fill value: random (%zu..%zu)\n"), 0, fill_value_max));
  else
    _log(wxString::Format(_("Fill value: %zu\n"), fill_value));

         // get fill value increment
  size_t fill_value_increment = 0;
  if (!m_DCMGenFillIncrementValue->GetValue().ToULongLong(&fill_value_increment)) {
    fill_value_increment = 0;
  } else if (fill_value_increment > fill_value_max) {
    fill_value_increment = fill_value_max;
  }
  m_DCMGenFillIncrementValue->SetValue(wxString::Format(wxT("%zu"), fill_value_increment));
  _log(wxString::Format(_("Fill value increment: %zu\n"), fill_value_increment));

         // get frames copy flag
  bool increment_for_new_frames_only = m_DCMGenFillIncrementForNewFrames->GetValue();
  _log(wxString::Format(_("Increment for new frames only: %s\n"),
                        increment_for_new_frames_only ? _("YES") : _("NO")));


  try {
    tape_scanner::dicom_generator::CreateDcm(file_path, width, height, bits, frames, fill_value, fill_value_max,
                                             fill_value_increment, increment_for_new_frames_only);

    _log(wxString::Format(_("Successfuly created. See - %s\n"), file_path));
  } catch (std::runtime_error &e) {
    _log(_(e.what()));
  }
  catch (std::bad_alloc &e){
    _log(_("ERROR: Failed to allocate RAM buffer.\n"));
  }
  catch( ... ){
    _log(_("ERROR: unexpected error occured when create dicom file"));
  }
}

wxString MyMainWindow::get_time_str() {
  time_t tt;
  struct tm* ti;
  time(&tt);
  ti = localtime(&tt);
  asctime(ti);
  return wxString(asctime(ti));
}

void MyMainWindow::_settings_file_update(bool set_status) {
  mINI::INIFile settings_file(settings_file_path);
  if (settings_file.write(settings, true)) {
    if (set_status) SetStatusText(_("Settings file successfully updated"), 2);
  } else {
    if (set_status) SetStatusText(_("Failed to update settings file"), 2);
  }
}

void MyMainWindow::_settings_file_create(bool set_status) {
  mINI::INIFile settings_file(settings_file_path);
  if (settings_file.generate(settings, true)) {
    if (set_status) SetStatusText(_("Settings file successfully created"), 2);
  } else {
    if (set_status) SetStatusText(_("Failed to create settings file"), 2);
  }
}

void MyMainWindow::_log(wxString str) {
  if (!m_log || !str) return;
  if(*(str.rbegin()) != '\n')
    str+= '\n';
  m_log->AppendText(str);
  m_log->SetInsertionPoint(-1);
  m_log->ScrollLines(999999);
}

void MyMainWindow::_controller_shell_log(wxString str) {
  if (!m_controller_shell_log || !str) return;
  m_controller_shell_log->AppendText(str);
  m_controller_shell_log->SetInsertionPoint(-1);
  m_controller_shell_log->ScrollLines(999999);
}

int MyMainWindow::_sendto_controller(const char *data, int size) {
  if (controller_socket == INVALID_SOCKET) return -1;
  return sendto(controller_socket, data, size, 0,
                (SOCKADDR *)&controller_socket_address, sizeof(controller_socket_address));
}

int MyMainWindow::_recvfrom_controller(char *data, int size) {
  if (controller_socket == INVALID_SOCKET) return -1;
  int address_size = sizeof(controller_socket_address);
  return recvfrom(controller_socket, data, size, 0,
                  (SOCKADDR *)&controller_socket_address, &address_size);
}

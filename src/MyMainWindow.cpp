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
#include <algorithm>
#include <cmath>
#include <string>
#include <cstring>

double safeStrToDouble(const wxString &value) {
  return safeStrToDouble(value.ToStdString());
}

int safeStrToInt(const wxString &value) {
  return safeStrToInt(value.ToStdString());
}
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
    settings["scan"]["bin"] = scan_bin_default.c_str().AsChar();
    settings["scan"]["pixel_um_x"] = scan_pixel_um_x_default.c_str().AsChar();
    settings["scan"]["pixel_um_y"] = scan_pixel_um_y_default.c_str().AsChar();
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
  // ppm остаётся в файле для старых настроек и не задаёт ни шаг каретки, ни Pixel Spacing.
  if (!settings.has("scan") || !settings["scan"].has("bin")) {
    settings["scan"]["bin"] = scan_bin_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("scan") || !settings["scan"].has("pixel_um_x")) {
    settings["scan"]["pixel_um_x"] = scan_pixel_um_x_default.c_str().AsChar();
    update = true;
  }
  if (!settings.has("scan") || !settings["scan"].has("pixel_um_y")) {
    settings["scan"]["pixel_um_y"] = scan_pixel_um_y_default.c_str().AsChar();
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
  SetupScanGeometryControls();
  if (create) _settings_file_create(true);
  if (update) _settings_file_update(true);
  // start timers
  m_service_timer.Start( 1000 );
  m_controller_timer.Start( 2000 );
  m_camera_timer.Start( 1000 );

  m_window_level_scroll_bar->SetRange(65536);
  m_window_level_scroll_bar->SetThumbPosition(500);

  m_window_width_scroll_bar->SetRange(65536);
  m_window_width_scroll_bar->SetThumbPosition(500);
  SetupViewerTools();

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
  if (m_scan_bin_choice) {
    const wxString bin_value = m_scan_bin_choice->GetSelection() == 1 ? wxT("1") : wxT("2");
    if (autosave_scan_bin != bin_value) {
      autosave_scan_bin = bin_value;
      settings["scan"]["bin"] = autosave_scan_bin.c_str();
      update++;
    }
  }
  if (m_pixel_um_x_text && autosave_pixel_um_x != m_pixel_um_x_text->GetValue()) {
    autosave_pixel_um_x = m_pixel_um_x_text->GetValue();
    settings["scan"]["pixel_um_x"] = autosave_pixel_um_x.c_str();
    update++;
  }
  if (m_pixel_um_y_text && autosave_pixel_um_y != m_pixel_um_y_text->GetValue()) {
    autosave_pixel_um_y = m_pixel_um_y_text->GetValue();
    settings["scan"]["pixel_um_y"] = autosave_pixel_um_y.c_str();
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
  ReloadScanBinChoice();
  settings["settings"]["language"] = "English";
  _settings_file_update();
}

void MyMainWindow::OnLanguageRussian(wxCommandEvent& event) {
  lng->set(wxT("Russian"));
  lng->translate_frame(this, wxT("LAST"), wxT("CURRENT"));
  ReloadScanBinChoice();
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

void MyMainWindow::ReloadScanBinChoice() {
  if (!m_scan_bin_choice || m_updating_scan_mode) {
    return;
  }
  m_updating_scan_mode = true;

  const auto read_um = [](wxTextCtrl *box) {
    if (!box) {
      return kNominalPixelUm;
    }
    const double value = safeStrToDouble(box->GetValue());
    return value > 0.0 ? value : kNominalPixelUm;
  };
  const double um_x = read_um(m_pixel_um_x_text);
  const double um_y = read_um(m_pixel_um_y_text);
  const wxString unit = (lng && lng->get() == wxT("Russian")) ? wxT("мкм") : wxT("um");
  const int sel = m_scan_bin_choice->GetSelection();

  auto label = [&](int bin, const wxString &dpi) {
    const std::string pitch_x = FormatPitchUm(um_x, bin);
    const std::string pitch_y = FormatPitchUm(um_y, bin);
    const wxString x_text = wxString::FromUTF8(pitch_x.c_str());
    const wxString y_text = wxString::FromUTF8(pitch_y.c_str());
    if (pitch_x == pitch_y) {
      return wxString::Format("%s (%s %s)", dpi, x_text, unit);
    }
    return wxString::Format("%s (%s x %s %s)", dpi, x_text, y_text, unit);
  };

  m_scan_bin_choice->Clear();
  m_scan_bin_choice->Append(label(2, wxT("600 dpi")));
  m_scan_bin_choice->Append(label(1, wxT("1200 dpi")));
  m_scan_bin_choice->SetSelection(sel == 1 ? 1 : 0);
  m_updating_scan_mode = false;
}

void MyMainWindow::SetupScanGeometryControls() {
  wxWindow *scan_box = m_checkBox_light_only_when_capture->GetParent();
  wxSizer *scan_sizer = nullptr;
  if (scan_box && m_panel3 && m_panel3->GetSizer()) {
    wxSizer *outer = m_panel3->GetSizer();
    for (size_t i = 0; i < outer->GetItemCount(); ++i) {
      wxSizerItem *item = outer->GetItem(i);
      if (!item->IsSizer()) {
        continue;
      }
      auto *box_sizer = dynamic_cast<wxStaticBoxSizer *>(item->GetSizer());
      if (box_sizer && box_sizer->GetStaticBox() == scan_box) {
        scan_sizer = box_sizer;
        break;
      }
    }
  }
  if (!scan_sizer) {
    return;
  }

  auto labelOf = [this](const wxString &english) {
    return lng->translate(english, wxT("English"), wxT("CURRENT"));
  };

  m_pixel_um_x_text = new wxTextCtrl(scan_box, wxID_ANY);
  m_pixel_um_x_text->SetMinSize(wxSize(80, -1));
  m_pixel_um_y_text = new wxTextCtrl(scan_box, wxID_ANY);
  m_pixel_um_y_text->SetMinSize(wxSize(80, -1));
  m_scan_bin_choice = new wxChoice(scan_box, wxID_ANY);
  m_scan_bin_choice->SetMinSize(wxSize(240, -1));

  auto makeRow = [&](const wxString &label, wxWindow *field) {
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *text = new wxStaticText(scan_box, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    row->Add(text, 0, wxALL, 8);
    row->Add(field, 0, wxALL, 5);
    return row;
  };

  int insert_at = static_cast<int>(scan_sizer->GetItemCount());
  for (size_t i = 0; i < scan_sizer->GetItemCount(); ++i) {
    wxSizerItem *item = scan_sizer->GetItem(i);
    if (item->IsSizer() && item->GetSizer()->GetItem(m_dst_scan_file_path)) {
      insert_at = static_cast<int>(i);
      break;
    }
  }

  scan_sizer->Insert(insert_at, makeRow(labelOf(wxT("Resolution:")), m_scan_bin_choice), 0, wxEXPAND, 5);
  scan_sizer->Insert(insert_at + 1, makeRow(labelOf(wxT("Pixel pitch X, um:")), m_pixel_um_x_text), 0, wxEXPAND, 5);
  scan_sizer->Insert(insert_at + 2, makeRow(labelOf(wxT("Pixel pitch Y, um:")), m_pixel_um_y_text), 0, wxEXPAND, 5);

  auto makeRulerRow = [&](const wxString &label, wxTextCtrl *&mm, wxTextCtrl *&pixels, const wxString &apply_label, bool along_x) {
    mm = new wxTextCtrl(scan_box, wxID_ANY);
    pixels = new wxTextCtrl(scan_box, wxID_ANY);
    mm->SetMinSize(wxSize(70, -1));
    pixels->SetMinSize(wxSize(70, -1));
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(scan_box, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT), 0, wxALL, 8);
    row->Add(mm, 0, wxALL, 5);
    row->Add(new wxStaticText(scan_box, wxID_ANY, labelOf(wxT("px:"))), 0, wxALL, 8);
    row->Add(pixels, 0, wxALL, 5);
    auto *apply = new wxButton(scan_box, wxID_ANY, apply_label);
    apply->Bind(wxEVT_BUTTON, [this, along_x](wxCommandEvent &) { ApplyRulerPitch(along_x); });
    row->Add(apply, 0, wxALL, 5);
    return row;
  };
  scan_sizer->Insert(insert_at + 3,
                     makeRulerRow(labelOf(wxT("Ruler X, mm:")), m_ruler_mm_x, m_ruler_px_x, labelOf(wxT("Apply X")), true),
                     0, wxEXPAND, 5);
  scan_sizer->Insert(insert_at + 4,
                     makeRulerRow(labelOf(wxT("Ruler Y, mm:")), m_ruler_mm_y, m_ruler_px_y, labelOf(wxT("Apply Y")), false),
                     0, wxEXPAND, 5);
  scan_sizer->Insert(insert_at + 5,
                     new wxStaticText(scan_box, wxID_ANY,
                                      labelOf(wxT("Pixels are counted on the saved grid; optical pitch divides by bin."))),
                     0, wxALL, 8);

  m_pixel_um_x_text->SetValue(wxString(settings["scan"]["pixel_um_x"]));
  m_pixel_um_y_text->SetValue(wxString(settings["scan"]["pixel_um_y"]));
  m_scan_bin_choice->Append(wxT("600 dpi"));
  m_scan_bin_choice->Append(wxT("1200 dpi"));
  m_scan_bin_choice->SetSelection(std::string(settings["scan"]["bin"]) == "1" ? 1 : 0);
  ReloadScanBinChoice();
  m_pixel_um_x_text->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { ReloadScanBinChoice(); });
  m_pixel_um_y_text->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { ReloadScanBinChoice(); });
  m_scan_bin_choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { ReloadScanBinChoice(); });
  autosave_scan_bin = m_scan_bin_choice->GetSelection() == 1 ? wxT("1") : wxT("2");
  autosave_pixel_um_x = m_pixel_um_x_text->GetValue();
  autosave_pixel_um_y = m_pixel_um_y_text->GetValue();

  scan_box->Layout();
  if (m_panel3) {
    m_panel3->Layout();
  }
  Layout();
}

ScanGeometry MyMainWindow::CurrentScanGeometry() const {
  ScanGeometry geometry;
  geometry.bin = (m_scan_bin_choice && m_scan_bin_choice->GetSelection() == 1) ? 1 : kDefaultBin;
  if (m_pixel_um_x_text) {
    geometry.pixel_um_x = safeStrToDouble(m_pixel_um_x_text->GetValue().ToStdString());
  }
  if (m_pixel_um_y_text) {
    geometry.pixel_um_y = safeStrToDouble(m_pixel_um_y_text->GetValue().ToStdString());
  }
  return ScanGeometry::Normalized(geometry);
}

void MyMainWindow::OnScanStart(wxCommandEvent &event) {
  if(!InitScanner()) {
    return;
  }

  double steps_by_peer_pix_x = safeStrToDouble(m_steps_by_per_pix_x_text->GetValue().ToStdString());
  double steps_by_peer_pix_y = safeStrToDouble(m_steps_by_per_pix_y_text->GetValue().ToStdString());
  m_scanner->SetStepsByPerPixel(steps_by_peer_pix_x, steps_by_peer_pix_y);

  const double pitch_x =
      m_pixel_um_x_text ? safeStrToDouble(m_pixel_um_x_text->GetValue().ToStdString()) : kNominalPixelUm;
  const double pitch_y =
      m_pixel_um_y_text ? safeStrToDouble(m_pixel_um_y_text->GetValue().ToStdString()) : kNominalPixelUm;
  if (!(pitch_x > 0.0) || !(pitch_y > 0.0)) {
    wxLogWarning("Pixel pitch must be positive; nominal 21 um is used");
  }
  m_scanner->SetScanGeometry(CurrentScanGeometry());

  m_scanner->StartScan(m_dst_scan_file_path->GetPath().ToStdString());
}

void MyMainWindow::ApplyRulerPitch(bool along_x) {
  wxTextCtrl *mm_box = along_x ? m_ruler_mm_x : m_ruler_mm_y;
  wxTextCtrl *px_box = along_x ? m_ruler_px_x : m_ruler_px_y;
  wxTextCtrl *dest = along_x ? m_pixel_um_x_text : m_pixel_um_y_text;
  if (!mm_box || !px_box || !dest) {
    return;
  }
  const int bin = (m_scan_bin_choice && m_scan_bin_choice->GetSelection() == 1) ? 1 : kDefaultBin;
  double optical_um = 0;
  if (!OpticalPitchFromSavedGrid(safeStrToDouble(mm_box->GetValue()), safeStrToDouble(px_box->GetValue()), bin, optical_um)) {
    wxLogWarning("Ruler distance and pixel count must be positive");
    return;
  }
  dest->SetValue(wxString::FromUTF8(FormatPitchUm(optical_um, 1)));
}

void MyMainWindow::OnRulerPixels(bool along_x, double pixels) {
  wxTextCtrl *px_box = along_x ? m_ruler_px_x : m_ruler_px_y;
  if (px_box) {
    px_box->SetValue(wxString::FromUTF8(FormatPitchUm(pixels, 1)));
  }
  m_updating_ruler = true;
  if (m_pick_x_btn) {
    m_pick_x_btn->SetValue(false);
  }
  if (m_pick_y_btn) {
    m_pick_y_btn->SetValue(false);
  }
  m_updating_ruler = false;
  wxTextCtrl *mm_box = along_x ? m_ruler_mm_x : m_ruler_mm_y;
  if (mm_box && safeStrToDouble(mm_box->GetValue()) > 0.0) {
    ApplyRulerPitch(along_x);
  }
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
    const WindowLevel fitted = m_dcm_processor.CurrentWindow();
    ShowWindow(static_cast<int>(std::lround(fitted.center)), static_cast<int>(std::lround(fitted.width)));
    open_file_rotation = 0;
    open_file_flip = 0;
    m_current_frame->SetValue(wxString::Format("%i", m_dcm_processor.GetCurFrameNum() + 1));
    m_rotation0_btn->SetValue(true);
    m_rotation90Left_btn->SetValue(false);
    m_rotation90Right_btn->SetValue(false);
    m_rotation180_btn->SetValue(false);
    m_flipH_btn->SetValue(false);
    m_flipV_btn->SetValue(false);
    if (m_invert_btn) {
      m_invert_btn->SetValue(false);
    }
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
  if (m_updating_window) {
    return;
  }
  int level = open_file_window_level;
  m_frame_window_level->GetValue().ToInt(&level);
  ShowWindow(level, open_file_window_width);
  m_dcm_processor.SetWindow(open_file_window_level, open_file_window_width);
}

void MyMainWindow::OnWindowLevelScroll(wxScrollEvent& win_level_data){
  if (m_updating_window) {
    return;
  }
  ShowWindow(win_level_data.GetInt(), open_file_window_width);
  m_dcm_processor.SetWindow(open_file_window_level, open_file_window_width);
}

void MyMainWindow::OnWindowWidthChange( wxCommandEvent& event ) {
  if (m_updating_window) {
    return;
  }
  int width = open_file_window_width;
  m_frame_window_width->GetValue().ToInt(&width);
  ShowWindow(open_file_window_level, width);
  m_dcm_processor.SetWindow(open_file_window_level, open_file_window_width);
}

void MyMainWindow::OnWindowWidthScroll(wxScrollEvent& win_width_data){
  if (m_updating_window) {
    return;
  }
  ShowWindow(open_file_window_level, win_width_data.GetInt());
  m_dcm_processor.SetWindow(open_file_window_level, open_file_window_width);
}

void MyMainWindow::OnFrameRotate0( wxCommandEvent& event ) {
  open_file_rotation = 0;
  m_rotation0_btn->SetValue(true);
  m_rotation90Left_btn->SetValue(false);
  m_rotation90Right_btn->SetValue(false);
  m_rotation180_btn->SetValue(false);
  ApplyOrientation();
}

void MyMainWindow::OnFrameRotate90Left( wxCommandEvent& event ) {
  open_file_rotation = -90;
  m_rotation0_btn->SetValue(false);
  m_rotation90Left_btn->SetValue(true);
  m_rotation90Right_btn->SetValue(false);
  m_rotation180_btn->SetValue(false);
  ApplyOrientation();
}

void MyMainWindow::OnFrameRotate90Right( wxCommandEvent& event ) {
  open_file_rotation = 90;
  m_rotation0_btn->SetValue(false);
  m_rotation90Left_btn->SetValue(false);
  m_rotation90Right_btn->SetValue(true);
  m_rotation180_btn->SetValue(false);
  ApplyOrientation();
}

void MyMainWindow::OnFrameRotate180( wxCommandEvent& event ) {
  open_file_rotation = 180;
  m_rotation0_btn->SetValue(false);
  m_rotation90Left_btn->SetValue(false);
  m_rotation90Right_btn->SetValue(false);
  m_rotation180_btn->SetValue(true);
  ApplyOrientation();
}

void MyMainWindow::OnFrameFlipHorizontally( wxCommandEvent& event ) {
  if (!m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 0;}
  if (m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 1;}
  if (!m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = 2;}
  if (m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = -1;}
  ApplyOrientation();
}

void MyMainWindow::OnFrameFlipVertically( wxCommandEvent& event ) {
  if (!m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 0;}
  if (m_flipH_btn->GetValue() && !m_flipV_btn->GetValue()) {open_file_flip = 1;}
  if (!m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = 2;}
  if (m_flipH_btn->GetValue() && m_flipV_btn->GetValue()) {open_file_flip = -1;}
  ApplyOrientation();
}

void MyMainWindow::SetupViewerTools() {
  if (m_flipH_btn && m_flipH_btn->GetContainingSizer()) {
    m_invert_btn = new wxToggleButton(m_flipH_btn->GetParent(), wxID_ANY, _("Invert"));
    m_flipH_btn->GetContainingSizer()->Add(m_invert_btn, 0, wxALL, 5);
    m_invert_btn->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) {
      m_dcm_processor.SetInvert(m_invert_btn->GetValue());
    });
    m_flipH_btn->GetParent()->Layout();
  }
  if (m_flipH_btn && m_flipH_btn->GetContainingSizer()) {
    auto *parent = m_flipH_btn->GetParent();
    auto *sizer = m_flipH_btn->GetContainingSizer();
    auto labelOf = [this](const wxString &english) {
      return lng->translate(english, wxT("English"), wxT("CURRENT"));
    };
    auto arm = [this](wxToggleButton *mine, wxToggleButton *other, bool along_x) {
      if (m_updating_ruler) {
        return;
      }
      if (!m_gl_image_canvas) {
        return;
      }
      if (mine->GetValue()) {
        m_updating_ruler = true;
        if (other) {
          other->SetValue(false);
        }
        m_updating_ruler = false;
        m_gl_image_canvas->BeginRuler(along_x);
      } else {
        m_gl_image_canvas->CancelRuler();
      }
    };
    m_pick_x_btn = new wxToggleButton(parent, wxID_ANY, labelOf(wxT("Pick X")));
    m_pick_y_btn = new wxToggleButton(parent, wxID_ANY, labelOf(wxT("Pick Y")));
    sizer->Add(m_pick_x_btn, 0, wxALL, 5);
    sizer->Add(m_pick_y_btn, 0, wxALL, 5);
    m_pick_x_btn->Bind(wxEVT_TOGGLEBUTTON, [this, arm](wxCommandEvent &) { arm(m_pick_x_btn, m_pick_y_btn, true); });
    m_pick_y_btn->Bind(wxEVT_TOGGLEBUTTON, [this, arm](wxCommandEvent &) { arm(m_pick_y_btn, m_pick_x_btn, false); });
    parent->Layout();
  }
  if (m_gl_image_canvas) {
    m_gl_image_canvas->SetWindowHandler([this](int center, int width) { ShowWindow(center, width); });
    m_gl_image_canvas->SetRulerHandler([this](bool along_x, double pixels) { OnRulerPixels(along_x, pixels); });
  }
}

void MyMainWindow::ShowWindow(int center, int width) {
  m_updating_window = true;
  open_file_window_level = std::clamp(center, 0, 65535);
  open_file_window_width = std::clamp(width, 1, 65535);
  if (m_frame_window_level) {
    m_frame_window_level->ChangeValue(wxString::Format("%i", open_file_window_level));
  }
  if (m_frame_window_width) {
    m_frame_window_width->ChangeValue(wxString::Format("%i", open_file_window_width));
  }
  if (m_window_level_scroll_bar) {
    m_window_level_scroll_bar->SetThumbPosition(open_file_window_level);
  }
  if (m_window_width_scroll_bar) {
    m_window_width_scroll_bar->SetThumbPosition(open_file_window_width);
  }
  m_updating_window = false;
}

void MyMainWindow::ApplyOrientation() {
  m_dcm_processor.SetOrientation(open_file_rotation, open_file_flip);
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
    auto result_dcm_file_path = tape_scanner::dicom_generator::ConvertJpegToDcm(m_in_jpeg_file_path.ToStdString());
    _log(wxString::Format(_("Successfuly converted. See - %s\n"), result_dcm_file_path));
  }
  catch( std::runtime_error& error)
  {
    _log(wxString(error.what()));
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

  const wxString dest_file_path = file_path + ".dcm";
  try {
    using namespace tape_scanner::dicom_generator;
    const ScanGeometry geometry = CurrentScanGeometry();
    const std::vector<std::string> written =
        ConvertBinToDcm(file_path.ToStdString(), width, height, bits, frames, all_frames_as_1st,
                        [this](const std::string &msg) { _log(msg); }, dest_file_path.ToStdString(),
                        geometry.RowSpacingMm(), geometry.ColSpacingMm());
    wxString names;
    for (const std::string &path : written) {
      if (!names.empty()) {
        names += wxT("\n");
      }
      names += wxString::FromUTF8(path);
    }
    _log(wxString::Format(_("Successfuly converted. See - %s\n"), names));
  }
  catch (const std::runtime_error &error) {
    _log(wxString(error.what()));
  }
  catch (const std::bad_alloc &) {
    _log(_("ERROR: Failed to allocate RAM buffer.\n"));
  }
  catch (...) {
    _log(_("ERROR: unexpected error occured when convert bin to dicom"));
  }
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
    const std::vector<std::string> written = tape_scanner::dicom_generator::CreateDcm(
        file_path.ToStdString(), width, height, bits, frames, fill_value, fill_value_max, fill_value_increment,
        increment_for_new_frames_only);
    wxString names;
    for (const std::string &path : written) {
      if (!names.empty()) {
        names += wxT("\n");
      }
      names += wxString::FromUTF8(path);
    }
    _log(wxString::Format(_("Successfuly created. See - %s\n"), names));
  } catch (std::runtime_error &e) {
    _log(wxString(e.what()));
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

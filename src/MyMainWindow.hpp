#pragma once

#include <wx/wx.h>

#include "MainWindow.h"

#include <tape_scanner/utils/language.h>
#include <tape_scanner/utils/ini.h>
#include <camera_api/CameraApi.h>
#include <tape_scanner/dicom_viewer/DicomProcessor.h>
#include <tape_scanner/scanner/Controller.hpp>
#include <tape_scanner/scanner/Camera.hpp>
#include <tape_scanner/scanner/Scanner.hpp>
#include <memory.h>

enum {
  TAB_NONE = -1,
  TAB_VIEWER = 0,
  TAB_DATA,
  TAB_GENERATOR,
  TAB_CONTROLLER,
  TAB_CAMERA,
  TAB_SCAN,
  TABS_COUNT
};

enum {
  SCAN_STATE_IDLE = 0,
  SCAN_STATE_START,
  SCAN_STATE_CAMERA_HOME_SEARCH,
  SCAN_STATE_GOTO_START_POS,
  SCAN_STATE_GOTO_NEXT_POS,
  SCAN_STATE_GOTO_HOME_POS,
  SCAN_STATE_CNT
};

class MyMainWindow : public MainWindow
{
public:
  MyMainWindow(wxWindow* parent);
  ~MyMainWindow();

protected:
  void OnServiceTimer( wxTimerEvent& event ) override;
  void OnControllerTimer( wxTimerEvent& event ) override;
  void OnCameraTimer( wxTimerEvent& event ) override;
  void OnExit(wxCommandEvent& event) override;
  void OnAbout(wxCommandEvent& event) override;
  void OnLanguageEnglish(wxCommandEvent& event) override;
  void OnLanguageRussian(wxCommandEvent& event) override;
  void OnTabChange(wxNotebookEvent& event) override;
  void OnLightOnlyWhenCapture( wxCommandEvent& event ) override;
  void OnScanStart( wxCommandEvent& event ) override;
  void OnScanStop( wxCommandEvent& event ) override;
  void OnSaveFrameAsRGB8bitmap( wxCommandEvent& event ) override;
  void OnSaveFrameAsMonoRAW16( wxCommandEvent& event ) override;
  void OnCameraSettingsSave( wxCommandEvent& event ) override;
  void OnControllerSettingsSave( wxCommandEvent& event ) override;
  void OnControllerInitBtn( wxCommandEvent& event ) override;
  void OnControllerShellCmdInput( wxCommandEvent& event ) override;
  void OnControllerShellClear( wxCommandEvent& event ) override;
  void OnOutput0Check( wxCommandEvent& event ) override;
  void OnOutput1Check( wxCommandEvent& event ) override;
  void OnMotor0SetPos( wxCommandEvent& event ) override;
  void OnMotor0GoToPos( wxCommandEvent& event ) override;
  void OnMotor0DownBtnClick( wxCommandEvent& event ) override;
  void OnMotor0UpBtnClick( wxCommandEvent& event ) override;
  void OnMotor0Home( wxCommandEvent& event ) override;
  void OnMotor0Stop( wxCommandEvent& event ) override;
  void OnMotor1SetPos( wxCommandEvent& event ) override;
  void OnMotor1GoToPos( wxCommandEvent& event ) override;
  void OnMotor1DownBtnClick( wxCommandEvent& event ) override;
  void OnMotor1UpBtnClick( wxCommandEvent& event ) override;
  void OnMotor1Home( wxCommandEvent& event ) override;
  void OnMotor1Stop( wxCommandEvent& event ) override;
  void OnCalibrateCamera( wxCommandEvent& event )  override;
  void OnCalculateOffset( wxCommandEvent& event )  override;

  void OnDCMFileOpen(wxCommandEvent& event) override;
  void OnOpenScan(wxCommandEvent& event);
  void OpenLoadedDicom(const wxString &path, const wxString &name);
  void OnCurrentFrameChange( wxCommandEvent& event ) override;
  void OnCurrentFramePrev( wxCommandEvent& event ) override;
  void OnCurrentFrameNext( wxCommandEvent& event ) override;
  void OnWindowLevelChange( wxCommandEvent& event ) override;
  void OnWindowLevelScroll(wxScrollEvent& win_level_data) override;
  void OnWindowWidthChange( wxCommandEvent& event ) override;
  void OnWindowWidthScroll(wxScrollEvent& win_width_data) override;
  void OnFrameRotate0( wxCommandEvent& event ) override;
  void OnFrameRotate90Left( wxCommandEvent& event ) override;
  void OnFrameRotate90Right( wxCommandEvent& event ) override;
  void OnFrameRotate180( wxCommandEvent& event ) override;
  void OnFrameFlipHorizontally( wxCommandEvent& event ) override;
  void OnFrameFlipVertically( wxCommandEvent& event ) override;
  void OnJPGToDCMConvert( wxCommandEvent& event ) override;
  void OnBINToDCMConvert( wxCommandEvent& event ) override;
  void OnDCMCreate( wxCommandEvent& event ) override;

private:
  void SetupDcmGenerator();
  void SetupScanGeometryControls();
  void SetupViewerTools();
  void ShowWindow(int center, int width);
  void ApplyOrientation();
  void ReloadScanBinChoice();
  void ApplyRulerPitch(bool along_x);
  void OnRulerPixels(bool along_x, double pixels);
  bool InitController();
  bool InitCamera();
  bool InitScanner();
  wxString get_time_str();
  void _settings_file_update(bool set_status = false);
  void _settings_file_create(bool set_status = false);
  void _log(wxString str);
  void _controller_shell_log(wxString str);
  int _sendto_controller(const char *data, int size);
  int _recvfrom_controller(char *data, int size);

         // gui state
  int tab = TAB_VIEWER;
  // multilingual support
  Language *lng;
  // settings storage
  const string settings_file_path = "settings.ini";
  mINI::INIStructure settings;
  // controller communication settings
  WSADATA wsa_data;
  SOCKET controller_socket = INVALID_SOCKET;
  SOCKADDR_IN controller_socket_address;
  size_t controller_init_cmd_pos = 0;
  int controller_init_last_cmd_answered = 0;
  int controller_init_cmds_failed = 0;
  char controller_init_last_cmd[128] = {0};
  int controller_init_cmd_timeout = 0;
  int controller_init_cmd_timeout_max = 1000;
  int controller_connection_timeout = 0;
  int controller_connection_timeout_max = 3000;
  char controller_shell_last_cmd[2048] = {0};
  // camera communication settings
  int camera_id = -1;
  tSdkCameraCapbility camera_info = {0};
  BYTE *camera_frame_buffer = NULL;
  UINT camera_frame_buffer_size = 0;
  int camera_connected = 0;
  int camera_connection_timeout = 0;
  int camera_connection_timeout_max = 3000;
  // default_settings
  wxString controller_ip_address_default  = "192.168.1.200";
  wxString controller_net_mask_default    = "255.255.255.0";
  wxString controller_gateway_default     = "192.168.1.1";
  wxString controller_udp_port_default    = "1000";
  wxString controller_init_cmds_default   = "// Inputs\ngpio PA10 type -1\ngpio PA9 type -1\n// Outputs\ngpio PE0 type 1\ngpio PE1 type 1\ngpio PE0 state 0\ngpio PE1 state 0\n// Camera\nstepdir 0 steppin PA0\nstepdir 0 dirpin PA1\nstepdir 0 limit0pin PA6\nstepdir 0 limit1pin PA4\nstepdir 0 steplen 5000\nstepdir 0 stepmult 160.0\nstepdir 0 accel 10000.0\nstepdir 0 minspeed 10.0\nstepdir 0 maxspeed 300.0\nstepdir 0 pos 0.0\nstepdir 0 posmin -9999.0\nstepdir 0 posmax 9999.0\nstepdir 0 state 1\n// Feeder\nstepdir 1 steppin PB0\nstepdir 1 dirpin PB1\nstepdir 1 limit0pin PC0\nstepdir 1 limit1pin PC1\nstepdir 1 steplen 5000\nstepdir 1 stepmult 162.0\nstepdir 1 accel 1000.0\nstepdir 1 minspeed 1.0\nstepdir 1 maxspeed 100.0\nstepdir 1 pos 0.0\nstepdir 1 posmin -9999.0\nstepdir 1 posmax 9999.0\nstepdir 1 state 1\n";
  wxString camera_ip_address_default      = "192.168.1.243";
  wxString camera_net_mask_default        = "255.255.255.0";
  wxString camera_gateway_default         = "192.168.1.1";
  bool camera_dhcp_default                = false;
  wxString camera_exposure_time_default   = "30000";
  wxString scan_width_default             = "100.0";
  wxString scan_height_default            = "100.0";
  wxString scan_ppm_default               = "36.571429";
  wxString scan_bin_default               = "2";
  wxString scan_pixel_um_x_default        = "21.0";
  wxString scan_pixel_um_y_default        = "21.0";
  // autosave
  wxString autosave_controller_ip_address;
  wxString autosave_controller_net_mask;
  wxString autosave_controller_gateway;
  wxString autosave_controller_udp_port;
  wxString autosave_controller_init_cmds;
  wxString autosave_camera_ip_address;
  wxString autosave_camera_net_mask;
  wxString autosave_camera_gateway;
  bool autosave_camera_dhcp;
  wxString autosave_camera_exposure_time;
  wxString autosave_scan_width;
  wxString autosave_scan_height;
  wxString autosave_scan_ppm;
  wxString autosave_scan_bin;
  wxString autosave_pixel_um_x;
  wxString autosave_pixel_um_y;
  wxChoice* m_scan_bin_choice = nullptr;
  wxTextCtrl* m_pixel_um_x_text = nullptr;
  wxTextCtrl* m_pixel_um_y_text = nullptr;
  wxTextCtrl* m_ruler_mm_x = nullptr;
  wxTextCtrl* m_ruler_px_x = nullptr;
  wxTextCtrl* m_ruler_mm_y = nullptr;
  wxTextCtrl* m_ruler_px_y = nullptr;
  wxToggleButton* m_pick_x_btn = nullptr;
  wxToggleButton* m_pick_y_btn = nullptr;
  bool m_updating_scan_mode = false;
  bool m_updating_ruler = false;
  // opened file data
  wxString open_file_path = "";
  wxString open_file_name = "";
  wxString open_file_tmp_path = "";
  int open_file_frame_width = 0;
  int open_file_frame_height = 0;
  int open_file_frames_cnt = 1;
  int open_file_frame = 1;
  int open_file_window_level = 128;
  int open_file_window_width = 256;
  int open_file_rotation = 0;
  int open_file_flip = 0;
  bool m_updating_window = false;
  wxToggleButton* m_invert_btn = nullptr;

  // Dicom viewer
  DicomProcessor m_dcm_processor;

  // Scanning
  std::shared_ptr<Controller> m_controller;
  std::shared_ptr<Camera> m_camera;
  std::unique_ptr<Scanner> m_scanner;

};

///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/intl.h>
#include "tape_scanner/dicom_viewer/GLImgViewer.h"
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/statusbr.h>
#include <wx/timer.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/scrolbar.h>
#include <wx/tglbtn.h>
#include <wx/statbox.h>
#include <wx/checkbox.h>
#include <wx/filepicker.h>
#include <wx/panel.h>
#include <wx/dataview.h>
#include <wx/choice.h>
#include <wx/statbmp.h>
#include <wx/scrolwin.h>
#include <wx/notebook.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class MainWindow
///////////////////////////////////////////////////////////////////////////////
class MainWindow : public wxFrame
{
	private:

	protected:
		wxMenuBar* m_menubar1;
		wxMenu* m_menu12;
		wxMenu* m_menu3;
		wxStatusBar* m_statusBar1;
		wxTimer m_service_timer;
		wxTimer m_controller_timer;
		wxTimer m_camera_timer;
		wxPanel* m_panel3;
		wxButton* m_button12;
		wxStaticText* m_frames_cnt_text;
		wxTextCtrl* m_current_frame;
		wxButton* m_button303;
		wxButton* m_button3012;
		wxStaticText* m_staticText7112231111253;
		wxTextCtrl* m_frame_window_level;
		wxScrollBar* m_window_level_scroll_bar;
		wxStaticText* m_staticText71122311112522;
		wxTextCtrl* m_frame_window_width;
		wxScrollBar* m_window_width_scroll_bar;
		wxStaticText* m_staticText7112231111252111;
		wxToggleButton* m_rotation90Left_btn;
		wxToggleButton* m_rotation0_btn;
		wxToggleButton* m_rotation90Right_btn;
		wxToggleButton* m_rotation180_btn;
		wxStaticText* m_staticText71122311112521111;
		wxToggleButton* m_flipH_btn;
		wxToggleButton* m_flipV_btn;
		wxStaticText* m_staticText7112231111252;
		wxTextCtrl* m_scan_width_text;
		wxStaticText* m_staticText7112231111251;
		wxStaticText* m_staticText711223111125;
		wxTextCtrl* m_scan_length_text;
		wxStaticText* m_staticText71122311112511;
		wxStaticText* m_staticText_steps_by_per_pix_x;
		wxTextCtrl* m_steps_by_per_pix_x_text;
		wxStaticText* m_staticText_m_steps_by_per_pix_y;
		wxTextCtrl* m_steps_by_per_pix_y_text;
		wxCheckBox* m_checkBox_light_only_when_capture;
		wxStaticText* selectDestFile_label;
		wxFilePickerCtrl* m_dst_scan_file_path;
		wxButton* m_start_scan_btn;
		wxButton* m_stop_scan_btn;
		wxTextCtrl* m_log;
		wxNotebook* m_tabs;
		wxPanel* m_panel_viewer;
		GLImgViewer* m_gl_image_canvas;
		wxPanel* m_panel_data;
		wxDataViewListCtrl* m_tags_view;
		wxDataViewColumn* m_dataViewListColumn1;
		wxDataViewColumn* m_dataViewListColumn2;
		wxDataViewColumn* m_dataViewListColumn3;
		wxPanel* m_panel_generator;
		wxStaticText* m_staticText7;
		wxFilePickerCtrl* m_JPGFilePicker;
		wxButton* m_button1;
		wxStaticText* m_staticText71;
		wxFilePickerCtrl* m_BINFilePicker;
		wxButton* m_button11;
		wxStaticText* m_staticText711223;
		wxTextCtrl* m_BINWidth;
		wxStaticText* m_staticText7111123;
		wxStaticText* m_staticText7112231;
		wxTextCtrl* m_BINHeight;
		wxStaticText* m_staticText71111231;
		wxStaticText* m_staticText71122311;
		wxChoice* m_BINBits;
		wxStaticText* m_staticText711112312;
		wxStaticText* m_staticText711223111;
		wxTextCtrl* m_BINFrames;
		wxCheckBox* m_BINAllFramesAs1stFrame;
		wxStaticText* m_staticText711;
		wxFilePickerCtrl* m_DCMSaveFilePicker;
		wxButton* m_button111;
		wxStaticText* m_staticText7112232;
		wxTextCtrl* m_DCMGenWidth;
		wxStaticText* m_staticText71111232;
		wxStaticText* m_staticText71122312;
		wxTextCtrl* m_DCMGenHeight;
		wxStaticText* m_staticText711112313;
		wxStaticText* m_staticText711223112;
		wxChoice* m_DCMGenBits;
		wxStaticText* m_staticText7111123121;
		wxStaticText* m_staticText7112231111;
		wxTextCtrl* m_DCMGenFrames;
		wxStaticText* m_staticText71122311111;
		wxTextCtrl* m_DCMGenFillValue;
		wxStaticText* m_staticText71111231211;
		wxStaticText* m_staticText711223111111;
		wxTextCtrl* m_DCMGenFillIncrementValue;
		wxCheckBox* m_DCMGenFillIncrementForNewFrames;
		wxPanel* m_panel_controller;
		wxStaticText* m_staticText71122311112;
		wxTextCtrl* m_controller_ip_address_text;
		wxStaticText* m_staticText711223111121;
		wxTextCtrl* m_controller_net_mask_text;
		wxStaticText* m_staticText711223111122;
		wxTextCtrl* m_controller_gateway_text;
		wxStaticText* m_staticText7112231111221;
		wxTextCtrl* m_controller_udp_port_text;
		wxStaticText* m_staticText71122311112212;
		wxButton* m_controller_settings_save;
		wxTextCtrl* m_controller_init_cmds_text;
		wxButton* m_controller_init_btn;
		wxTextCtrl* m_controller_shell_log;
		wxButton* m_controller_shell_clear_btn;
		wxTextCtrl* m_controller_shell_cmd_text;
		wxButton* m_controller_shell_send_btn;
		wxStaticText* m_staticText71122311112211;
		wxCheckBox* m_input0_check;
		wxCheckBox* m_input1_check;
		wxStaticText* m_staticText711223111122111;
		wxCheckBox* m_output0_check;
		wxCheckBox* m_output1_check;
		wxStaticText* calibrations_lbl;
		wxButton* m_calibrate_camera_btn;
		wxButton* m_calculate_offsets_btn;
		wxStaticText* m_staticText711223111123;
		wxStaticText* m_staticText37;
		wxTextCtrl* m_motor0_pos_text;
		wxStaticText* m_staticText371;
		wxTextCtrl* m_motor0_goto_text;
		wxButton* m_motor0_home_btn;
		wxToggleButton* m_motor0_down_tbtn;
		wxToggleButton* m_motor0_up_tbtn;
		wxButton* m_motor0_stop_btn;
		wxStaticText* m_staticText7112231111231;
		wxStaticText* m_staticText372;
		wxTextCtrl* m_motor1_pos_text;
		wxStaticText* m_staticText3711;
		wxTextCtrl* m_motor1_goto_text;
		wxButton* m_motor1_home_btn;
		wxToggleButton* m_motor1_down_tbtn;
		wxToggleButton* m_motor1_up_tbtn;
		wxButton* m_motor1_stop_btn;
		wxPanel* m_panel_camera;
		wxStaticText* m_staticText711223111124;
		wxTextCtrl* m_camera_ip_address_text;
		wxStaticText* m_staticText7112231111211;
		wxTextCtrl* m_camera_net_mask_text;
		wxStaticText* m_staticText7112231111222;
		wxTextCtrl* m_camera_gateway_text;
		wxStaticText* m_staticText71122311112213;
		wxCheckBox* m_camera_dhcp_checkbox;
		wxStaticText* m_staticText711223111122121;
		wxButton* m_camera_settings_save;
		wxStaticText* m_staticText7112231111241;
		wxTextCtrl* m_camera_exposure_time_text;
		wxStaticText* m_staticText71122311112411;
		wxButton* m_camera_action_btn1;
		wxButton* m_camera_action_btn11;
		wxStaticBitmap* m_camera_bitmap;
		wxPanel* m_panel_scan;
		wxScrolledWindow* m_scan_scroll_box;
		wxFlexGridSizer* fgScanPreviewSizer;

		// Virtual event handlers, override them in your derived class
		virtual void OnDCMFileOpen( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDCMFileSaveAs( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDCMFileClose( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnExit( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnLanguageEnglish( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnLanguageRussian( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAbout( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnServiceTimer( wxTimerEvent& event ) { event.Skip(); }
		virtual void OnControllerTimer( wxTimerEvent& event ) { event.Skip(); }
		virtual void OnCameraTimer( wxTimerEvent& event ) { event.Skip(); }
		virtual void OnCurrentFrameChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCurrentFramePrev( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCurrentFrameNext( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnWindowLevelChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnWindowLevelScroll( wxScrollEvent& event ) { event.Skip(); }
		virtual void OnWindowWidthChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnWindowWidthScroll( wxScrollEvent& event ) { event.Skip(); }
		virtual void OnFrameRotate90Left( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFrameRotate0( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFrameRotate90Right( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFrameRotate180( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFrameFlipHorizontally( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFrameFlipVertically( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnLightOnlyWhenCapture( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnScanStart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnScanStop( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnTabChange( wxNotebookEvent& event ) { event.Skip(); }
		virtual void OnJPGToDCMConvert( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnBINToDCMConvert( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDCMCreate( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnControllerSettingsSave( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnControllerInitBtn( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnControllerShellClear( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnControllerShellCmdInput( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOutput0Check( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOutput1Check( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCalibrateCamera( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCalculateOffset( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor0SetPos( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor0GoToPos( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor0Home( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor0DownBtnClick( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor0UpBtnClick( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor0Stop( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor1SetPos( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor1GoToPos( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor1Home( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor1DownBtnClick( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor1UpBtnClick( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnMotor1Stop( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCameraSettingsSave( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSaveFrameAsRGB8bitmap( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSaveFrameAsMonoRAW16( wxCommandEvent& event ) { event.Skip(); }


	public:
		wxMenu* m_menu1;

		MainWindow( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("Medical films scanner"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 1280,800 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

		~MainWindow();

};


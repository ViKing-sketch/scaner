///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 4.2.1-0-g80c4cb6)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "MainWindow.h"

///////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 640,480 ), wxDefaultSize );

	m_menubar1 = new wxMenuBar( 0 );
	m_menu1 = new wxMenu();
	wxMenuItem* m_menuItem1;
	m_menuItem1 = new wxMenuItem( m_menu1, wxID_ANY, wxString( _("Open DCM File..") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu1->Append( m_menuItem1 );

	wxMenuItem* m_menuItem13;
	m_menuItem13 = new wxMenuItem( m_menu1, wxID_ANY, wxString( _("Save DCM file as..") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu1->Append( m_menuItem13 );
	m_menuItem13->Enable( false );

	wxMenuItem* m_menuItem11;
	m_menuItem11 = new wxMenuItem( m_menu1, wxID_ANY, wxString( _("Close DCM file") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu1->Append( m_menuItem11 );
	m_menuItem11->Enable( false );

	m_menu1->AppendSeparator();

	wxMenuItem* m_menuItem3;
	m_menuItem3 = new wxMenuItem( m_menu1, wxID_ANY, wxString( _("Quit") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu1->Append( m_menuItem3 );

	m_menubar1->Append( m_menu1, _("File") );

	m_menu12 = new wxMenu();
	wxMenuItem* m_menuItem12;
	m_menuItem12 = new wxMenuItem( m_menu12, wxID_ANY, wxString( _("English") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu12->Append( m_menuItem12 );

	wxMenuItem* m_menuItem21;
	m_menuItem21 = new wxMenuItem( m_menu12, wxID_ANY, wxString( _("Russian") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu12->Append( m_menuItem21 );

	m_menubar1->Append( m_menu12, _("Language") );

	m_menu3 = new wxMenu();
	wxMenuItem* m_menuItem5;
	m_menuItem5 = new wxMenuItem( m_menu3, wxID_ANY, wxString( _("About") ) , wxEmptyString, wxITEM_NORMAL );
	m_menu3->Append( m_menuItem5 );

	m_menubar1->Append( m_menu3, _("Help") );

	this->SetMenuBar( m_menubar1 );

	m_statusBar1 = this->CreateStatusBar( 3, wxSTB_SIZEGRIP, wxID_ANY );
	m_service_timer.SetOwner( this, m_service_timer.GetId() );
	m_controller_timer.SetOwner( this, m_controller_timer.GetId() );
	m_camera_timer.SetOwner( this, m_camera_timer.GetId() );
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxHORIZONTAL );

	m_panel3 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel3->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	wxBoxSizer* bSizer81;
	bSizer81 = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sbSizer8;
	sbSizer8 = new wxStaticBoxSizer( new wxStaticBox( m_panel3, wxID_ANY, _("DICOM file actions") ), wxVERTICAL );

	m_button12 = new wxButton( sbSizer8->GetStaticBox(), wxID_ANY, _("Open DCM file.."), wxDefaultPosition, wxDefaultSize, 0 );
	sbSizer8->Add( m_button12, 0, wxALL, 5 );

	wxFlexGridSizer* fgSizer81;
	fgSizer81 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer81->SetFlexibleDirection( wxBOTH );
	fgSizer81->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_frames_cnt_text = new wxStaticText( sbSizer8->GetStaticBox(), wxID_ANY, _("Frame:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_frames_cnt_text->Wrap( 1 );
	fgSizer81->Add( m_frames_cnt_text, 0, wxALL, 8 );

	wxBoxSizer* bSizer6811135111;
	bSizer6811135111 = new wxBoxSizer( wxHORIZONTAL );

	m_current_frame = new wxTextCtrl( sbSizer8->GetStaticBox(), wxID_ANY, _("1"), wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_current_frame->SetMinSize( wxSize( 50,-1 ) );

	bSizer6811135111->Add( m_current_frame, 0, wxALL, 5 );

	m_button303 = new wxButton( sbSizer8->GetStaticBox(), wxID_ANY, _("<<"), wxDefaultPosition, wxSize( 40,-1 ), 0 );
	bSizer6811135111->Add( m_button303, 0, wxALL, 5 );

	m_button3012 = new wxButton( sbSizer8->GetStaticBox(), wxID_ANY, _(">>"), wxDefaultPosition, wxSize( 40,-1 ), 0 );
	bSizer6811135111->Add( m_button3012, 0, wxALL, 5 );


	fgSizer81->Add( bSizer6811135111, 1, wxEXPAND, 5 );

	m_staticText7112231111253 = new wxStaticText( sbSizer8->GetStaticBox(), wxID_ANY, _("Window Level:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111253->Wrap( 1 );
	fgSizer81->Add( m_staticText7112231111253, 0, wxALL, 8 );

	wxBoxSizer* bSizer68111352;
	bSizer68111352 = new wxBoxSizer( wxHORIZONTAL );

	m_frame_window_level = new wxTextCtrl( sbSizer8->GetStaticBox(), wxID_ANY, _("500"), wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_frame_window_level->SetMinSize( wxSize( 50,-1 ) );

	bSizer68111352->Add( m_frame_window_level, 1, wxALL, 5 );

	m_window_level_scroll_bar = new wxScrollBar( sbSizer8->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_HORIZONTAL );
	bSizer68111352->Add( m_window_level_scroll_bar, 3, wxALL, 5 );


	fgSizer81->Add( bSizer68111352, 2, wxEXPAND, 0 );

	m_staticText71122311112522 = new wxStaticText( sbSizer8->GetStaticBox(), wxID_ANY, _("Window Width:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112522->Wrap( 1 );
	fgSizer81->Add( m_staticText71122311112522, 0, wxALL, 8 );

	wxBoxSizer* bSizer681113512;
	bSizer681113512 = new wxBoxSizer( wxHORIZONTAL );

	m_frame_window_width = new wxTextCtrl( sbSizer8->GetStaticBox(), wxID_ANY, _("500"), wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_frame_window_width->SetMinSize( wxSize( 50,-1 ) );

	bSizer681113512->Add( m_frame_window_width, 0, wxALL, 5 );

	m_window_width_scroll_bar = new wxScrollBar( sbSizer8->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_HORIZONTAL );
	bSizer681113512->Add( m_window_width_scroll_bar, 2, wxALL, 5 );


	fgSizer81->Add( bSizer681113512, 1, wxEXPAND, 5 );

	m_staticText7112231111252111 = new wxStaticText( sbSizer8->GetStaticBox(), wxID_ANY, _("Rotation:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111252111->Wrap( 1 );
	fgSizer81->Add( m_staticText7112231111252111, 0, wxALL, 8 );

	wxBoxSizer* bSizer681113521;
	bSizer681113521 = new wxBoxSizer( wxHORIZONTAL );

	m_rotation90Left_btn = new wxToggleButton( sbSizer8->GetStaticBox(), wxID_ANY, _("-90"), wxDefaultPosition, wxSize( 40,-1 ), 0 );
	bSizer681113521->Add( m_rotation90Left_btn, 0, wxALL, 5 );

	m_rotation0_btn = new wxToggleButton( sbSizer8->GetStaticBox(), wxID_ANY, _("0"), wxDefaultPosition, wxSize( 30,-1 ), 0 );
	m_rotation0_btn->SetValue( true );
	bSizer681113521->Add( m_rotation0_btn, 0, wxALL, 5 );

	m_rotation90Right_btn = new wxToggleButton( sbSizer8->GetStaticBox(), wxID_ANY, _("+90"), wxDefaultPosition, wxSize( 40,-1 ), 0 );
	bSizer681113521->Add( m_rotation90Right_btn, 0, wxALL, 5 );

	m_rotation180_btn = new wxToggleButton( sbSizer8->GetStaticBox(), wxID_ANY, _("180"), wxDefaultPosition, wxSize( 40,-1 ), 0 );
	bSizer681113521->Add( m_rotation180_btn, 0, wxALL, 5 );


	fgSizer81->Add( bSizer681113521, 1, wxEXPAND, 5 );

	m_staticText71122311112521111 = new wxStaticText( sbSizer8->GetStaticBox(), wxID_ANY, _("Flip:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112521111->Wrap( 1 );
	fgSizer81->Add( m_staticText71122311112521111, 0, wxALL, 8 );

	wxBoxSizer* bSizer6811135211;
	bSizer6811135211 = new wxBoxSizer( wxHORIZONTAL );

	m_flipH_btn = new wxToggleButton( sbSizer8->GetStaticBox(), wxID_ANY, _("Horizontally"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer6811135211->Add( m_flipH_btn, 0, wxALL, 5 );

	m_flipV_btn = new wxToggleButton( sbSizer8->GetStaticBox(), wxID_ANY, _("Vertically"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer6811135211->Add( m_flipV_btn, 0, wxALL, 5 );


	fgSizer81->Add( bSizer6811135211, 1, wxEXPAND, 5 );


	sbSizer8->Add( fgSizer81, 1, wxEXPAND, 0 );


	bSizer81->Add( sbSizer8, 0, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer81;
	sbSizer81 = new wxStaticBoxSizer( new wxStaticBox( m_panel3, wxID_ANY, _("Scanning") ), wxVERTICAL );

	wxBoxSizer* bSizer6811135;
	bSizer6811135 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText7112231111252 = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("Film length:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111252->Wrap( 1 );
	bSizer6811135->Add( m_staticText7112231111252, 0, wxALL, 8 );

	m_scan_width_text = new wxTextCtrl( sbSizer81->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_scan_width_text->SetMinSize( wxSize( 80,-1 ) );

	bSizer6811135->Add( m_scan_width_text, 0, wxALL, 5 );

	m_staticText7112231111251 = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("mm"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111251->Wrap( 1 );
	bSizer6811135->Add( m_staticText7112231111251, 0, wxALL, 8 );


	sbSizer81->Add( bSizer6811135, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer68111351;
	bSizer68111351 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText711223111125 = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("Film width: "), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111125->Wrap( 1 );
	bSizer68111351->Add( m_staticText711223111125, 0, wxALL, 8 );

	m_scan_length_text = new wxTextCtrl( sbSizer81->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_scan_length_text->SetMinSize( wxSize( 80,-1 ) );

	bSizer68111351->Add( m_scan_length_text, 0, wxALL, 5 );

	m_staticText71122311112511 = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("mm"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112511->Wrap( 1 );
	bSizer68111351->Add( m_staticText71122311112511, 0, wxALL, 8 );


	sbSizer81->Add( bSizer68111351, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer68111353;
	bSizer68111353 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText_steps_by_per_pix_x = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("Pix size hor"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText_steps_by_per_pix_x->Wrap( 1 );
	bSizer68111353->Add( m_staticText_steps_by_per_pix_x, 0, wxALL, 8 );

	m_steps_by_per_pix_x_text = new wxTextCtrl( sbSizer81->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_steps_by_per_pix_x_text->SetMinSize( wxSize( 80,-1 ) );

	bSizer68111353->Add( m_steps_by_per_pix_x_text, 0, wxALL, 5 );


	sbSizer81->Add( bSizer68111353, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer681113531;
	bSizer681113531 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText_m_steps_by_per_pix_y = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("Pix size ver"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText_m_steps_by_per_pix_y->Wrap( 1 );
	bSizer681113531->Add( m_staticText_m_steps_by_per_pix_y, 0, wxALL, 8 );

	m_steps_by_per_pix_y_text = new wxTextCtrl( sbSizer81->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_steps_by_per_pix_y_text->SetMinSize( wxSize( 80,-1 ) );

	bSizer681113531->Add( m_steps_by_per_pix_y_text, 0, wxALL, 5 );


	sbSizer81->Add( bSizer681113531, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer681113511;
	bSizer681113511 = new wxBoxSizer( wxHORIZONTAL );

	m_checkBox_light_only_when_capture = new wxCheckBox( sbSizer81->GetStaticBox(), wxID_ANY, _("Light only when capture"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer681113511->Add( m_checkBox_light_only_when_capture, 0, wxALL, 5 );


	sbSizer81->Add( bSizer681113511, 1, wxEXPAND, 5 );

	wxBoxSizer* selectDstFile;
	selectDstFile = new wxBoxSizer( wxHORIZONTAL );

	selectDestFile_label = new wxStaticText( sbSizer81->GetStaticBox(), wxID_ANY, _("Save as"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	selectDestFile_label->Wrap( 1 );
	selectDstFile->Add( selectDestFile_label, 0, wxALL, 5 );

	m_dst_scan_file_path = new wxFilePickerCtrl( sbSizer81->GetStaticBox(), wxID_ANY, wxT(".\\scan.dcm"), _("Select a file"), _("*.*"), wxDefaultPosition, wxDefaultSize, wxFLP_SAVE|wxFLP_USE_TEXTCTRL );
	selectDstFile->Add( m_dst_scan_file_path, 1, wxALL|wxEXPAND, 5 );


	sbSizer81->Add( selectDstFile, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer44;
	bSizer44 = new wxBoxSizer( wxHORIZONTAL );

	m_start_scan_btn = new wxButton( sbSizer81->GetStaticBox(), wxID_ANY, _("Start"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer44->Add( m_start_scan_btn, 0, wxALL, 5 );

	m_stop_scan_btn = new wxButton( sbSizer81->GetStaticBox(), wxID_ANY, _("Stop"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer44->Add( m_stop_scan_btn, 0, wxALL, 5 );


	bSizer44->Add( 0, 0, 1, wxEXPAND, 5 );


	sbSizer81->Add( bSizer44, 0, wxEXPAND, 0 );


	bSizer81->Add( sbSizer81, 0, wxALL|wxEXPAND, 5 );

	m_log = new wxTextCtrl( m_panel3, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_WORDWRAP );
	m_log->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_INFOBK ) );
	m_log->SetMinSize( wxSize( 200,200 ) );

	bSizer81->Add( m_log, 1, wxALL|wxEXPAND, 5 );


	m_panel3->SetSizer( bSizer81 );
	m_panel3->Layout();
	bSizer81->Fit( m_panel3 );
	bSizer1->Add( m_panel3, 30, wxEXPAND | wxALL, 0 );

	m_tabs = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_tabs->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );
	m_tabs->SetMinSize( wxSize( 400,400 ) );

	m_panel_viewer = new wxPanel( m_tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel_viewer->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_APPWORKSPACE ) );

	wxBoxSizer* bSizer32;
	bSizer32 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer621;
	bSizer621 = new wxBoxSizer( wxHORIZONTAL );

	m_gl_image_canvas = new GLImgViewer( m_panel_viewer);
	bSizer621->Add( m_gl_image_canvas, 1, wxALL|wxEXPAND, 5 );


	bSizer32->Add( bSizer621, 1, wxALL|wxEXPAND, 5 );


	m_panel_viewer->SetSizer( bSizer32 );
	m_panel_viewer->Layout();
	bSizer32->Fit( m_panel_viewer );
	m_tabs->AddPage( m_panel_viewer, _("Viewer"), false );
	m_panel_data = new wxPanel( m_tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel_data->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	wxBoxSizer* bSizer3111;
	bSizer3111 = new wxBoxSizer( wxHORIZONTAL );

	m_tags_view = new wxDataViewListCtrl( m_panel_data, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES );
	m_dataViewListColumn1 = m_tags_view->AppendTextColumn( _("(Group,Element)"), wxDATAVIEW_CELL_INERT, 100, static_cast<wxAlignment>(wxALIGN_LEFT), wxDATAVIEW_COL_RESIZABLE|wxDATAVIEW_COL_SORTABLE );
	m_dataViewListColumn2 = m_tags_view->AppendTextColumn( _("Description"), wxDATAVIEW_CELL_INERT, 220, static_cast<wxAlignment>(wxALIGN_LEFT), wxDATAVIEW_COL_RESIZABLE|wxDATAVIEW_COL_SORTABLE );
	m_dataViewListColumn3 = m_tags_view->AppendTextColumn( _("Value"), wxDATAVIEW_CELL_INERT, -1, static_cast<wxAlignment>(wxALIGN_LEFT), wxDATAVIEW_COL_RESIZABLE|wxDATAVIEW_COL_SORTABLE );
	bSizer3111->Add( m_tags_view, 1, wxALL|wxEXPAND, 5 );


	m_panel_data->SetSizer( bSizer3111 );
	m_panel_data->Layout();
	bSizer3111->Fit( m_panel_data );
	m_tabs->AddPage( m_panel_data, _("Data"), false );
	m_panel_generator = new wxPanel( m_tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel_generator->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	wxBoxSizer* bSizer31111;
	bSizer31111 = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sbSizer1;
	sbSizer1 = new wxStaticBoxSizer( new wxStaticBox( m_panel_generator, wxID_ANY, _("JPEG to colored DICOM image converter") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer3;
	fgSizer3 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer3->SetFlexibleDirection( wxBOTH );
	fgSizer3->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );

	m_staticText7 = new wxStaticText( sbSizer1->GetStaticBox(), wxID_ANY, _("Select JPEG file:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7->Wrap( -1 );
	m_staticText7->SetMinSize( wxSize( 120,-1 ) );

	fgSizer3->Add( m_staticText7, 1, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer8;
	bSizer8 = new wxBoxSizer( wxHORIZONTAL );

	m_JPGFilePicker = new wxFilePickerCtrl( sbSizer1->GetStaticBox(), wxID_ANY, wxEmptyString, _("Open JPEG file.."), _("JPEG files (*.jpg;*.jpeg)|*.jpg;*.jpeg"), wxDefaultPosition, wxDefaultSize, wxFLP_DEFAULT_STYLE );
	bSizer8->Add( m_JPGFilePicker, 0, wxALL, 5 );

	m_button1 = new wxButton( sbSizer1->GetStaticBox(), wxID_ANY, _("Convert"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer8->Add( m_button1, 0, wxALL, 5 );


	fgSizer3->Add( bSizer8, 1, wxEXPAND, 5 );


	sbSizer1->Add( fgSizer3, 1, wxEXPAND, 5 );


	bSizer31111->Add( sbSizer1, 0, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer11;
	sbSizer11 = new wxStaticBoxSizer( new wxStaticBox( m_panel_generator, wxID_ANY, _("Binary (RAW) to monochrome DICOM image converter") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer2;
	fgSizer2 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer2->SetFlexibleDirection( wxBOTH );
	fgSizer2->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );

	m_staticText71 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("Select binary file:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71->Wrap( -1 );
	m_staticText71->SetMinSize( wxSize( 120,-1 ) );

	fgSizer2->Add( m_staticText71, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer69;
	bSizer69 = new wxBoxSizer( wxHORIZONTAL );

	m_BINFilePicker = new wxFilePickerCtrl( sbSizer11->GetStaticBox(), wxID_ANY, wxEmptyString, _("Open binary file.."), _("Any files (*.*)|*.*"), wxDefaultPosition, wxDefaultSize, wxFLP_DEFAULT_STYLE );
	bSizer69->Add( m_BINFilePicker, 0, wxALL, 5 );

	m_button11 = new wxButton( sbSizer11->GetStaticBox(), wxID_ANY, _("Convert"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer69->Add( m_button11, 0, wxALL, 5 );


	fgSizer2->Add( bSizer69, 1, wxEXPAND, 5 );

	m_staticText711223 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("Width:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223->Wrap( 1 );
	fgSizer2->Add( m_staticText711223, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68;
	bSizer68 = new wxBoxSizer( wxHORIZONTAL );

	m_BINWidth = new wxTextCtrl( sbSizer11->GetStaticBox(), wxID_ANY, _("512"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_BINWidth->SetMinSize( wxSize( 60,-1 ) );

	bSizer68->Add( m_BINWidth, 0, wxALL, 5 );

	m_staticText7111123 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("px"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText7111123->Wrap( -1 );
	bSizer68->Add( m_staticText7111123, 0, wxALL, 8 );


	fgSizer2->Add( bSizer68, 1, wxEXPAND, 5 );

	m_staticText7112231 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("Height:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231->Wrap( 1 );
	fgSizer2->Add( m_staticText7112231, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer681;
	bSizer681 = new wxBoxSizer( wxHORIZONTAL );

	m_BINHeight = new wxTextCtrl( sbSizer11->GetStaticBox(), wxID_ANY, _("512"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_BINHeight->SetMinSize( wxSize( 60,-1 ) );

	bSizer681->Add( m_BINHeight, 0, wxALL, 5 );

	m_staticText71111231 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("px"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText71111231->Wrap( -1 );
	bSizer681->Add( m_staticText71111231, 0, wxALL, 8 );


	fgSizer2->Add( bSizer681, 1, wxEXPAND, 5 );

	m_staticText71122311 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("Color depth:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311->Wrap( 1 );
	fgSizer2->Add( m_staticText71122311, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6812;
	bSizer6812 = new wxBoxSizer( wxHORIZONTAL );

	wxString m_BINBitsChoices[] = { _("8"), _("16") };
	int m_BINBitsNChoices = sizeof( m_BINBitsChoices ) / sizeof( wxString );
	m_BINBits = new wxChoice( sbSizer11->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, m_BINBitsNChoices, m_BINBitsChoices, 0 );
	m_BINBits->SetSelection( 0 );
	m_BINBits->SetMinSize( wxSize( 60,-1 ) );

	bSizer6812->Add( m_BINBits, 0, wxALL, 5 );

	m_staticText711112312 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("bits"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText711112312->Wrap( -1 );
	bSizer6812->Add( m_staticText711112312, 0, wxALL, 8 );


	fgSizer2->Add( bSizer6812, 1, wxEXPAND, 5 );

	m_staticText711223111 = new wxStaticText( sbSizer11->GetStaticBox(), wxID_ANY, _("Frames:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111->Wrap( 1 );
	fgSizer2->Add( m_staticText711223111, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer681112;
	bSizer681112 = new wxBoxSizer( wxHORIZONTAL );

	m_BINFrames = new wxTextCtrl( sbSizer11->GetStaticBox(), wxID_ANY, _("1"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_BINFrames->SetMinSize( wxSize( 60,-1 ) );

	bSizer681112->Add( m_BINFrames, 0, wxALL, 5 );

	m_BINAllFramesAs1stFrame = new wxCheckBox( sbSizer11->GetStaticBox(), wxID_ANY, _("all frames as 1st frame"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer681112->Add( m_BINAllFramesAs1stFrame, 0, wxALL, 8 );


	fgSizer2->Add( bSizer681112, 1, wxEXPAND, 5 );


	sbSizer11->Add( fgSizer2, 1, wxEXPAND, 5 );


	bSizer31111->Add( sbSizer11, 0, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer111;
	sbSizer111 = new wxStaticBoxSizer( new wxStaticBox( m_panel_generator, wxID_ANY, _("Parametric monochrome DICOM image generator") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer21;
	fgSizer21 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer21->SetFlexibleDirection( wxBOTH );
	fgSizer21->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );

	m_staticText711 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Save DCM file as:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711->Wrap( -1 );
	m_staticText711->SetMinSize( wxSize( 120,-1 ) );

	fgSizer21->Add( m_staticText711, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer691;
	bSizer691 = new wxBoxSizer( wxHORIZONTAL );

	m_DCMSaveFilePicker = new wxFilePickerCtrl( sbSizer111->GetStaticBox(), wxID_ANY, wxEmptyString, _("Select output file.."), _("DICOM files (*.dcm)|*.dcm"), wxDefaultPosition, wxDefaultSize, wxFLP_SAVE|wxFLP_USE_TEXTCTRL );
	bSizer691->Add( m_DCMSaveFilePicker, 0, wxALL, 5 );

	m_button111 = new wxButton( sbSizer111->GetStaticBox(), wxID_ANY, _("Create"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer691->Add( m_button111, 0, wxALL, 5 );


	fgSizer21->Add( bSizer691, 1, wxEXPAND, 5 );

	m_staticText7112232 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Width:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112232->Wrap( 1 );
	fgSizer21->Add( m_staticText7112232, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer682;
	bSizer682 = new wxBoxSizer( wxHORIZONTAL );

	m_DCMGenWidth = new wxTextCtrl( sbSizer111->GetStaticBox(), wxID_ANY, _("512"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_DCMGenWidth->SetMinSize( wxSize( 60,-1 ) );

	bSizer682->Add( m_DCMGenWidth, 0, wxALL, 5 );

	m_staticText71111232 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("px"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText71111232->Wrap( -1 );
	bSizer682->Add( m_staticText71111232, 0, wxALL, 8 );


	fgSizer21->Add( bSizer682, 1, wxEXPAND, 5 );

	m_staticText71122312 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Height:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122312->Wrap( 1 );
	fgSizer21->Add( m_staticText71122312, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6813;
	bSizer6813 = new wxBoxSizer( wxHORIZONTAL );

	m_DCMGenHeight = new wxTextCtrl( sbSizer111->GetStaticBox(), wxID_ANY, _("512"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_DCMGenHeight->SetMinSize( wxSize( 60,-1 ) );

	bSizer6813->Add( m_DCMGenHeight, 0, wxALL, 5 );

	m_staticText711112313 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("px"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText711112313->Wrap( -1 );
	bSizer6813->Add( m_staticText711112313, 0, wxALL, 8 );


	fgSizer21->Add( bSizer6813, 1, wxEXPAND, 5 );

	m_staticText711223112 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Color depth:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223112->Wrap( 1 );
	fgSizer21->Add( m_staticText711223112, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68121;
	bSizer68121 = new wxBoxSizer( wxHORIZONTAL );

	wxString m_DCMGenBitsChoices[] = { _("8"), _("16") };
	int m_DCMGenBitsNChoices = sizeof( m_DCMGenBitsChoices ) / sizeof( wxString );
	m_DCMGenBits = new wxChoice( sbSizer111->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, m_DCMGenBitsNChoices, m_DCMGenBitsChoices, 0 );
	m_DCMGenBits->SetSelection( 0 );
	m_DCMGenBits->SetMinSize( wxSize( 60,-1 ) );

	bSizer68121->Add( m_DCMGenBits, 0, wxALL, 5 );

	m_staticText7111123121 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("bits"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText7111123121->Wrap( -1 );
	bSizer68121->Add( m_staticText7111123121, 0, wxALL, 8 );


	fgSizer21->Add( bSizer68121, 1, wxEXPAND, 5 );

	m_staticText7112231111 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Frames:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111->Wrap( 1 );
	fgSizer21->Add( m_staticText7112231111, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68111;
	bSizer68111 = new wxBoxSizer( wxHORIZONTAL );

	m_DCMGenFrames = new wxTextCtrl( sbSizer111->GetStaticBox(), wxID_ANY, _("1"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_DCMGenFrames->SetMinSize( wxSize( 60,-1 ) );

	bSizer68111->Add( m_DCMGenFrames, 0, wxALL, 5 );


	fgSizer21->Add( bSizer68111, 1, wxEXPAND, 5 );

	m_staticText71122311111 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Fill value:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311111->Wrap( 1 );
	fgSizer21->Add( m_staticText71122311111, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer681111;
	bSizer681111 = new wxBoxSizer( wxHORIZONTAL );

	m_DCMGenFillValue = new wxTextCtrl( sbSizer111->GetStaticBox(), wxID_ANY, _("0"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_DCMGenFillValue->SetMinSize( wxSize( 60,-1 ) );

	bSizer681111->Add( m_DCMGenFillValue, 0, wxALL, 5 );

	m_staticText71111231211 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("(any byte/word value, not a number = random value)"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText71111231211->Wrap( -1 );
	bSizer681111->Add( m_staticText71111231211, 0, wxALL, 8 );


	fgSizer21->Add( bSizer681111, 1, wxEXPAND, 5 );

	m_staticText711223111111 = new wxStaticText( sbSizer111->GetStaticBox(), wxID_ANY, _("Fill value increment:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111111->Wrap( 1 );
	fgSizer21->Add( m_staticText711223111111, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6811111;
	bSizer6811111 = new wxBoxSizer( wxHORIZONTAL );

	m_DCMGenFillIncrementValue = new wxTextCtrl( sbSizer111->GetStaticBox(), wxID_ANY, _("0"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_DCMGenFillIncrementValue->SetMinSize( wxSize( 60,-1 ) );

	bSizer6811111->Add( m_DCMGenFillIncrementValue, 0, wxALL, 5 );

	m_DCMGenFillIncrementForNewFrames = new wxCheckBox( sbSizer111->GetStaticBox(), wxID_ANY, _("for new frames only"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer6811111->Add( m_DCMGenFillIncrementForNewFrames, 0, wxALL, 8 );


	fgSizer21->Add( bSizer6811111, 1, wxEXPAND, 5 );


	sbSizer111->Add( fgSizer21, 1, wxEXPAND, 5 );


	bSizer31111->Add( sbSizer111, 0, wxALL|wxEXPAND, 5 );


	m_panel_generator->SetSizer( bSizer31111 );
	m_panel_generator->Layout();
	bSizer31111->Fit( m_panel_generator );
	m_tabs->AddPage( m_panel_generator, _("DICOM files generator"), false );
	m_panel_controller = new wxPanel( m_tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel_controller->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	wxBoxSizer* bSizer311111;
	bSizer311111 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer29;
	bSizer29 = new wxBoxSizer( wxHORIZONTAL );

	wxStaticBoxSizer* sbSizer1111;
	sbSizer1111 = new wxStaticBoxSizer( new wxStaticBox( m_panel_controller, wxID_ANY, _("Network settings") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer211;
	fgSizer211 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer211->SetFlexibleDirection( wxBOTH );
	fgSizer211->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );

	m_staticText71122311112 = new wxStaticText( sbSizer1111->GetStaticBox(), wxID_ANY, _("IP address:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112->Wrap( 1 );
	fgSizer211->Add( m_staticText71122311112, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer681113;
	bSizer681113 = new wxBoxSizer( wxHORIZONTAL );

	m_controller_ip_address_text = new wxTextCtrl( sbSizer1111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_controller_ip_address_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer681113->Add( m_controller_ip_address_text, 0, wxALL, 5 );


	fgSizer211->Add( bSizer681113, 1, wxEXPAND, 5 );

	m_staticText711223111121 = new wxStaticText( sbSizer1111->GetStaticBox(), wxID_ANY, _("Network mask:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111121->Wrap( 1 );
	fgSizer211->Add( m_staticText711223111121, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6811131;
	bSizer6811131 = new wxBoxSizer( wxHORIZONTAL );

	m_controller_net_mask_text = new wxTextCtrl( sbSizer1111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_controller_net_mask_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer6811131->Add( m_controller_net_mask_text, 0, wxALL, 5 );


	fgSizer211->Add( bSizer6811131, 1, wxEXPAND, 5 );

	m_staticText711223111122 = new wxStaticText( sbSizer1111->GetStaticBox(), wxID_ANY, _("Gateway:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111122->Wrap( 1 );
	fgSizer211->Add( m_staticText711223111122, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6811132;
	bSizer6811132 = new wxBoxSizer( wxHORIZONTAL );

	m_controller_gateway_text = new wxTextCtrl( sbSizer1111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_controller_gateway_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer6811132->Add( m_controller_gateway_text, 0, wxALL, 5 );


	fgSizer211->Add( bSizer6811132, 1, wxEXPAND, 5 );

	m_staticText7112231111221 = new wxStaticText( sbSizer1111->GetStaticBox(), wxID_ANY, _("UDP port:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111221->Wrap( 1 );
	fgSizer211->Add( m_staticText7112231111221, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68111321;
	bSizer68111321 = new wxBoxSizer( wxHORIZONTAL );

	m_controller_udp_port_text = new wxTextCtrl( sbSizer1111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_controller_udp_port_text->SetMinSize( wxSize( 60,-1 ) );

	bSizer68111321->Add( m_controller_udp_port_text, 0, wxALL, 5 );


	fgSizer211->Add( bSizer68111321, 1, wxEXPAND, 5 );

	m_staticText71122311112212 = new wxStaticText( sbSizer1111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112212->Wrap( 1 );
	fgSizer211->Add( m_staticText71122311112212, 0, wxALL|wxEXPAND, 8 );

	m_controller_settings_save = new wxButton( sbSizer1111->GetStaticBox(), wxID_ANY, _("Save to controller"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer211->Add( m_controller_settings_save, 0, wxALL, 5 );


	sbSizer1111->Add( fgSizer211, 1, wxEXPAND, 5 );


	bSizer29->Add( sbSizer1111, 1, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer61;
	sbSizer61 = new wxStaticBoxSizer( new wxStaticBox( m_panel_controller, wxID_ANY, _("Initial commands") ), wxVERTICAL );

	m_controller_init_cmds_text = new wxTextCtrl( sbSizer61->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE );
	m_controller_init_cmds_text->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_INFOBK ) );
	m_controller_init_cmds_text->SetMinSize( wxSize( -1,120 ) );

	sbSizer61->Add( m_controller_init_cmds_text, 1, wxALL|wxEXPAND, 5 );

	m_controller_init_btn = new wxButton( sbSizer61->GetStaticBox(), wxID_ANY, _("Init"), wxDefaultPosition, wxDefaultSize, 0 );
	sbSizer61->Add( m_controller_init_btn, 0, wxALL, 5 );


	bSizer29->Add( sbSizer61, 1, wxALL|wxEXPAND, 5 );


	bSizer311111->Add( bSizer29, 0, wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer6;
	sbSizer6 = new wxStaticBoxSizer( new wxStaticBox( m_panel_controller, wxID_ANY, _("Shell") ), wxVERTICAL );

	m_controller_shell_log = new wxTextCtrl( sbSizer6->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE|wxTE_READONLY|wxTE_WORDWRAP );
	m_controller_shell_log->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_INFOBK ) );
	m_controller_shell_log->SetMinSize( wxSize( -1,120 ) );

	sbSizer6->Add( m_controller_shell_log, 0, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer34;
	bSizer34 = new wxBoxSizer( wxHORIZONTAL );

	m_controller_shell_clear_btn = new wxButton( sbSizer6->GetStaticBox(), wxID_ANY, _("Clear"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer34->Add( m_controller_shell_clear_btn, 0, wxALL, 5 );

	m_controller_shell_cmd_text = new wxTextCtrl( sbSizer6->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_controller_shell_cmd_text->SetMinSize( wxSize( 60,-1 ) );

	bSizer34->Add( m_controller_shell_cmd_text, 1, wxALL, 5 );

	m_controller_shell_send_btn = new wxButton( sbSizer6->GetStaticBox(), wxID_ANY, _("Send"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer34->Add( m_controller_shell_send_btn, 0, wxALL, 5 );


	sbSizer6->Add( bSizer34, 1, wxEXPAND, 5 );


	bSizer311111->Add( sbSizer6, 0, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer7;
	sbSizer7 = new wxStaticBoxSizer( new wxStaticBox( m_panel_controller, wxID_ANY, _("Control") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer2111;
	fgSizer2111 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer2111->SetFlexibleDirection( wxBOTH );
	fgSizer2111->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );

	m_staticText71122311112211 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("Inputs:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112211->Wrap( 1 );
	fgSizer2111->Add( m_staticText71122311112211, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer681113211;
	bSizer681113211 = new wxBoxSizer( wxHORIZONTAL );

	m_input0_check = new wxCheckBox( sbSizer7->GetStaticBox(), wxID_ANY, _("Film start"), wxDefaultPosition, wxDefaultSize, 0 );
	m_input0_check->Enable( false );

	bSizer681113211->Add( m_input0_check, 0, wxALL, 8 );

	m_input1_check = new wxCheckBox( sbSizer7->GetStaticBox(), wxID_ANY, _("24V"), wxDefaultPosition, wxDefaultSize, 0 );
	m_input1_check->Enable( false );

	bSizer681113211->Add( m_input1_check, 0, wxALL, 8 );


	fgSizer2111->Add( bSizer681113211, 1, wxEXPAND, 5 );

	m_staticText711223111122111 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("Outputs:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111122111->Wrap( 1 );
	fgSizer2111->Add( m_staticText711223111122111, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6811132111;
	bSizer6811132111 = new wxBoxSizer( wxHORIZONTAL );

	m_output0_check = new wxCheckBox( sbSizer7->GetStaticBox(), wxID_ANY, _("Backlight 0"), wxDefaultPosition, wxDefaultSize, 0 );
	m_output0_check->SetValue(true);
	bSizer6811132111->Add( m_output0_check, 0, wxALL, 8 );

	m_output1_check = new wxCheckBox( sbSizer7->GetStaticBox(), wxID_ANY, _("Backlight 1"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer6811132111->Add( m_output1_check, 0, wxALL, 8 );


	fgSizer2111->Add( bSizer6811132111, 1, wxEXPAND, 5 );

	calibrations_lbl = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("Calibrations:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	calibrations_lbl->Wrap( 1 );
	fgSizer2111->Add( calibrations_lbl, 0, wxALL, 5 );

	wxBoxSizer* calibrationis_bs;
	calibrationis_bs = new wxBoxSizer( wxHORIZONTAL );

	m_calibrate_camera_btn = new wxButton( sbSizer7->GetStaticBox(), wxID_ANY, _("Calibrate camera"), wxDefaultPosition, wxDefaultSize, 0 );
	calibrationis_bs->Add( m_calibrate_camera_btn, 0, wxALL, 5 );

	m_calculate_offsets_btn = new wxButton( sbSizer7->GetStaticBox(), wxID_ANY, _("Calculate offsets"), wxDefaultPosition, wxDefaultSize, 0 );
	calibrationis_bs->Add( m_calculate_offsets_btn, 0, wxALL, 5 );


	fgSizer2111->Add( calibrationis_bs, 1, wxEXPAND, 5 );

	m_staticText711223111123 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("Motor 0 (Camera):"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111123->Wrap( 1 );
	fgSizer2111->Add( m_staticText711223111123, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6811133;
	bSizer6811133 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText37 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("pos"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText37->Wrap( -1 );
	bSizer6811133->Add( m_staticText37, 0, wxALL, 8 );

	m_motor0_pos_text = new wxTextCtrl( sbSizer7->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_motor0_pos_text->SetMinSize( wxSize( 80,-1 ) );

	bSizer6811133->Add( m_motor0_pos_text, 0, wxALL, 5 );

	m_staticText371 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("go to"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText371->Wrap( -1 );
	bSizer6811133->Add( m_staticText371, 0, wxALL, 8 );

	m_motor0_goto_text = new wxTextCtrl( sbSizer7->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_motor0_goto_text->SetMinSize( wxSize( 72,-1 ) );

	bSizer6811133->Add( m_motor0_goto_text, 0, wxALL, 5 );

	m_motor0_home_btn = new wxButton( sbSizer7->GetStaticBox(), wxID_ANY, _(" Home "), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
	bSizer6811133->Add( m_motor0_home_btn, 0, wxALL, 5 );

	m_motor0_down_tbtn = new wxToggleButton( sbSizer7->GetStaticBox(), wxID_ANY, _("-"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_motor0_down_tbtn->SetMinSize( wxSize( 32,-1 ) );

	bSizer6811133->Add( m_motor0_down_tbtn, 0, wxALL, 5 );

	m_motor0_up_tbtn = new wxToggleButton( sbSizer7->GetStaticBox(), wxID_ANY, _("+"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_motor0_up_tbtn->SetMinSize( wxSize( 32,-1 ) );

	bSizer6811133->Add( m_motor0_up_tbtn, 0, wxALL, 5 );

	m_motor0_stop_btn = new wxButton( sbSizer7->GetStaticBox(), wxID_ANY, _(" Stop "), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
	bSizer6811133->Add( m_motor0_stop_btn, 0, wxALL, 5 );


	fgSizer2111->Add( bSizer6811133, 1, wxEXPAND, 5 );

	m_staticText7112231111231 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("Motor 1 (Feeder):"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111231->Wrap( 1 );
	fgSizer2111->Add( m_staticText7112231111231, 0, wxALL, 8 );

	wxBoxSizer* bSizer68111331;
	bSizer68111331 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText372 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("pos"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText372->Wrap( -1 );
	bSizer68111331->Add( m_staticText372, 0, wxALL, 8 );

	m_motor1_pos_text = new wxTextCtrl( sbSizer7->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_motor1_pos_text->SetMinSize( wxSize( 80,-1 ) );

	bSizer68111331->Add( m_motor1_pos_text, 0, wxALL, 5 );

	m_staticText3711 = new wxStaticText( sbSizer7->GetStaticBox(), wxID_ANY, _("go to"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText3711->Wrap( -1 );
	bSizer68111331->Add( m_staticText3711, 0, wxALL, 8 );

	m_motor1_goto_text = new wxTextCtrl( sbSizer7->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_PROCESS_ENTER );
	m_motor1_goto_text->SetMinSize( wxSize( 72,-1 ) );

	bSizer68111331->Add( m_motor1_goto_text, 0, wxALL, 5 );

	m_motor1_home_btn = new wxButton( sbSizer7->GetStaticBox(), wxID_ANY, _(" Home "), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
	bSizer68111331->Add( m_motor1_home_btn, 0, wxALL, 5 );

	m_motor1_down_tbtn = new wxToggleButton( sbSizer7->GetStaticBox(), wxID_ANY, _("-"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_motor1_down_tbtn->SetMinSize( wxSize( 32,-1 ) );

	bSizer68111331->Add( m_motor1_down_tbtn, 0, wxALL, 5 );

	m_motor1_up_tbtn = new wxToggleButton( sbSizer7->GetStaticBox(), wxID_ANY, _("+"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_motor1_up_tbtn->SetMinSize( wxSize( 32,-1 ) );

	bSizer68111331->Add( m_motor1_up_tbtn, 0, wxALL, 5 );

	m_motor1_stop_btn = new wxButton( sbSizer7->GetStaticBox(), wxID_ANY, _(" Stop "), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT );
	bSizer68111331->Add( m_motor1_stop_btn, 0, wxALL, 5 );


	fgSizer2111->Add( bSizer68111331, 1, wxEXPAND, 5 );


	sbSizer7->Add( fgSizer2111, 1, wxEXPAND, 5 );


	bSizer311111->Add( sbSizer7, 0, wxALL|wxEXPAND, 5 );


	m_panel_controller->SetSizer( bSizer311111 );
	m_panel_controller->Layout();
	bSizer311111->Fit( m_panel_controller );
	m_tabs->AddPage( m_panel_controller, _("Controller"), true );
	m_panel_camera = new wxPanel( m_tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panel_camera->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	wxBoxSizer* bSizer3111111;
	bSizer3111111 = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* bSizer291;
	bSizer291 = new wxBoxSizer( wxHORIZONTAL );

	wxStaticBoxSizer* sbSizer11111;
	sbSizer11111 = new wxStaticBoxSizer( new wxStaticBox( m_panel_camera, wxID_ANY, _("Network settings") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer2112;
	fgSizer2112 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer2112->SetFlexibleDirection( wxBOTH );
	fgSizer2112->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );

	m_staticText711223111124 = new wxStaticText( sbSizer11111->GetStaticBox(), wxID_ANY, _("IP address:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111124->Wrap( 1 );
	fgSizer2112->Add( m_staticText711223111124, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer6811134;
	bSizer6811134 = new wxBoxSizer( wxHORIZONTAL );

	m_camera_ip_address_text = new wxTextCtrl( sbSizer11111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_camera_ip_address_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer6811134->Add( m_camera_ip_address_text, 0, wxALL, 5 );


	fgSizer2112->Add( bSizer6811134, 1, wxEXPAND, 5 );

	m_staticText7112231111211 = new wxStaticText( sbSizer11111->GetStaticBox(), wxID_ANY, _("Network mask:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111211->Wrap( 1 );
	fgSizer2112->Add( m_staticText7112231111211, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68111311;
	bSizer68111311 = new wxBoxSizer( wxHORIZONTAL );

	m_camera_net_mask_text = new wxTextCtrl( sbSizer11111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_camera_net_mask_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer68111311->Add( m_camera_net_mask_text, 0, wxALL, 5 );


	fgSizer2112->Add( bSizer68111311, 1, wxEXPAND, 5 );

	m_staticText7112231111222 = new wxStaticText( sbSizer11111->GetStaticBox(), wxID_ANY, _("Gateway:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111222->Wrap( 1 );
	fgSizer2112->Add( m_staticText7112231111222, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68111322;
	bSizer68111322 = new wxBoxSizer( wxHORIZONTAL );

	m_camera_gateway_text = new wxTextCtrl( sbSizer11111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_camera_gateway_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer68111322->Add( m_camera_gateway_text, 0, wxALL, 5 );


	fgSizer2112->Add( bSizer68111322, 1, wxEXPAND, 5 );

	m_staticText71122311112213 = new wxStaticText( sbSizer11111->GetStaticBox(), wxID_ANY, _("Dynamic IP:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112213->Wrap( 1 );
	fgSizer2112->Add( m_staticText71122311112213, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer681113212;
	bSizer681113212 = new wxBoxSizer( wxHORIZONTAL );

	m_camera_dhcp_checkbox = new wxCheckBox( sbSizer11111->GetStaticBox(), wxID_ANY, _("Use DHCP client"), wxDefaultPosition, wxDefaultSize, 0 );
	bSizer681113212->Add( m_camera_dhcp_checkbox, 0, wxALL, 8 );


	fgSizer2112->Add( bSizer681113212, 1, wxEXPAND, 5 );

	m_staticText711223111122121 = new wxStaticText( sbSizer11111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText711223111122121->Wrap( 1 );
	fgSizer2112->Add( m_staticText711223111122121, 0, wxALL|wxEXPAND, 8 );

	m_camera_settings_save = new wxButton( sbSizer11111->GetStaticBox(), wxID_ANY, _("Save to camera"), wxDefaultPosition, wxDefaultSize, 0 );
	fgSizer2112->Add( m_camera_settings_save, 0, wxALL, 5 );


	sbSizer11111->Add( fgSizer2112, 1, wxEXPAND, 5 );


	bSizer291->Add( sbSizer11111, 1, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer111111;
	sbSizer111111 = new wxStaticBoxSizer( new wxStaticBox( m_panel_camera, wxID_ANY, _("Other settings") ), wxVERTICAL );

	wxFlexGridSizer* fgSizer21121;
	fgSizer21121 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer21121->SetFlexibleDirection( wxBOTH );
	fgSizer21121->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_ALL );

	m_staticText7112231111241 = new wxStaticText( sbSizer111111->GetStaticBox(), wxID_ANY, _("Exposure:"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText7112231111241->Wrap( 1 );
	fgSizer21121->Add( m_staticText7112231111241, 0, wxALL|wxEXPAND, 8 );

	wxBoxSizer* bSizer68111341;
	bSizer68111341 = new wxBoxSizer( wxHORIZONTAL );

	m_camera_exposure_time_text = new wxTextCtrl( sbSizer111111->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_camera_exposure_time_text->SetMinSize( wxSize( 120,-1 ) );

	bSizer68111341->Add( m_camera_exposure_time_text, 0, wxALL, 5 );

	m_staticText71122311112411 = new wxStaticText( sbSizer111111->GetStaticBox(), wxID_ANY, _("us"), wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
	m_staticText71122311112411->Wrap( 1 );
	bSizer68111341->Add( m_staticText71122311112411, 0, wxALL, 8 );


	fgSizer21121->Add( bSizer68111341, 1, wxEXPAND, 5 );


	sbSizer111111->Add( fgSizer21121, 1, wxEXPAND, 5 );


	bSizer291->Add( sbSizer111111, 1, wxALL|wxEXPAND, 5 );


	bSizer3111111->Add( bSizer291, 0, wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer13;
	sbSizer13 = new wxStaticBoxSizer( new wxStaticBox( m_panel_camera, wxID_ANY, _("Actions with current frame") ), wxHORIZONTAL );

	m_camera_action_btn1 = new wxButton( sbSizer13->GetStaticBox(), wxID_ANY, _("Save as RGB8 bitmap"), wxDefaultPosition, wxDefaultSize, 0 );
	sbSizer13->Add( m_camera_action_btn1, 0, wxALL, 5 );

	m_camera_action_btn11 = new wxButton( sbSizer13->GetStaticBox(), wxID_ANY, _("Save as monochrome RAW16"), wxDefaultPosition, wxDefaultSize, 0 );
	sbSizer13->Add( m_camera_action_btn11, 0, wxALL, 5 );


	bSizer3111111->Add( sbSizer13, 0, wxALL|wxEXPAND, 5 );

	wxStaticBoxSizer* sbSizer62;
	sbSizer62 = new wxStaticBoxSizer( new wxStaticBox( m_panel_camera, wxID_ANY, _("Preview") ), wxVERTICAL );

	wxBoxSizer* bSizer62;
	bSizer62 = new wxBoxSizer( wxHORIZONTAL );

	m_camera_bitmap = new wxStaticBitmap( sbSizer62->GetStaticBox(), wxID_ANY, wxBitmap( wxT("camera_preview.bmp"), wxBITMAP_TYPE_ANY ), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	bSizer62->Add( m_camera_bitmap, 0, wxALL, 5 );


	sbSizer62->Add( bSizer62, 1, wxEXPAND, 5 );


	bSizer3111111->Add( sbSizer62, 1, wxALL|wxEXPAND, 5 );


	m_panel_camera->SetSizer( bSizer3111111 );
	m_panel_camera->Layout();
	bSizer3111111->Fit( m_panel_camera );
	m_tabs->AddPage( m_panel_camera, _("Camera"), false );
	m_panel_scan = new wxPanel( m_tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_panel_scan->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	wxBoxSizer* bSizer321;
	bSizer321 = new wxBoxSizer( wxVERTICAL );

	wxStaticBoxSizer* sbSizer621;
	sbSizer621 = new wxStaticBoxSizer( new wxStaticBox( m_panel_scan, wxID_ANY, _("Preview") ), wxHORIZONTAL );

	m_scan_scroll_box = new wxScrolledWindow( sbSizer621->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_scan_scroll_box->SetScrollRate( 20, 20 );
	fgScanPreviewSizer = new wxFlexGridSizer( 0, 3, 0, 0 );
	fgScanPreviewSizer->SetFlexibleDirection( wxBOTH );
	fgScanPreviewSizer->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );


	m_scan_scroll_box->SetSizer( fgScanPreviewSizer );
	m_scan_scroll_box->Layout();
	fgScanPreviewSizer->Fit( m_scan_scroll_box );
	sbSizer621->Add( m_scan_scroll_box, 1, wxEXPAND | wxALL, 5 );


	bSizer321->Add( sbSizer621, 1, wxALL|wxEXPAND, 5 );


	m_panel_scan->SetSizer( bSizer321 );
	m_panel_scan->Layout();
	bSizer321->Fit( m_panel_scan );
	m_tabs->AddPage( m_panel_scan, _("Scanning"), false );

	bSizer1->Add( m_tabs, 70, wxEXPAND, 5 );


	this->SetSizer( bSizer1 );
	this->Layout();

	this->Centre( wxBOTH );

	// Connect Events
	m_menu1->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnDCMFileOpen ), this, m_menuItem1->GetId());
	m_menu1->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnDCMFileSaveAs ), this, m_menuItem13->GetId());
	m_menu1->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnDCMFileClose ), this, m_menuItem11->GetId());
	m_menu1->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnExit ), this, m_menuItem3->GetId());
	m_menu12->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnLanguageEnglish ), this, m_menuItem12->GetId());
	m_menu12->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnLanguageRussian ), this, m_menuItem21->GetId());
	m_menu3->Bind(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( MainWindow::OnAbout ), this, m_menuItem5->GetId());
	this->Connect( m_service_timer.GetId(), wxEVT_TIMER, wxTimerEventHandler( MainWindow::OnServiceTimer ) );
	this->Connect( m_controller_timer.GetId(), wxEVT_TIMER, wxTimerEventHandler( MainWindow::OnControllerTimer ) );
	this->Connect( m_camera_timer.GetId(), wxEVT_TIMER, wxTimerEventHandler( MainWindow::OnCameraTimer ) );
	m_button12->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnDCMFileOpen ), NULL, this );
	m_current_frame->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnCurrentFrameChange ), NULL, this );
	m_button303->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCurrentFramePrev ), NULL, this );
	m_button3012->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCurrentFrameNext ), NULL, this );
	m_frame_window_level->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnWindowLevelChange ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_TOP, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_BOTTOM, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_LINEUP, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_PAGEUP, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Connect( wxEVT_SCROLL_CHANGED, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_frame_window_width->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnWindowWidthChange ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_TOP, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_BOTTOM, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_LINEUP, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_PAGEUP, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Connect( wxEVT_SCROLL_CHANGED, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_rotation90Left_btn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate90Left ), NULL, this );
	m_rotation0_btn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate0 ), NULL, this );
	m_rotation90Right_btn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate90Right ), NULL, this );
	m_rotation180_btn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate180 ), NULL, this );
	m_flipH_btn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameFlipHorizontally ), NULL, this );
	m_flipV_btn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameFlipVertically ), NULL, this );
	m_checkBox_light_only_when_capture->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( MainWindow::OnLightOnlyWhenCapture ), NULL, this );
	m_start_scan_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnScanStart ), NULL, this );
	m_stop_scan_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnScanStop ), NULL, this );
	m_tabs->Connect( wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler( MainWindow::OnTabChange ), NULL, this );
	m_button1->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnJPGToDCMConvert ), NULL, this );
	m_button11->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnBINToDCMConvert ), NULL, this );
	m_button111->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnDCMCreate ), NULL, this );
	m_controller_settings_save->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerSettingsSave ), NULL, this );
	m_controller_init_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerInitBtn ), NULL, this );
	m_controller_shell_clear_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerShellClear ), NULL, this );
	m_controller_shell_cmd_text->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnControllerShellCmdInput ), NULL, this );
	m_controller_shell_send_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerShellCmdInput ), NULL, this );
	m_output0_check->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( MainWindow::OnOutput0Check ), NULL, this );
	m_output1_check->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( MainWindow::OnOutput1Check ), NULL, this );
	m_calibrate_camera_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCalibrateCamera ), NULL, this );
	m_calculate_offsets_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCalculateOffset ), NULL, this );
	m_motor0_pos_text->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor0SetPos ), NULL, this );
	m_motor0_goto_text->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor0GoToPos ), NULL, this );
	m_motor0_home_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0Home ), NULL, this );
	m_motor0_down_tbtn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0DownBtnClick ), NULL, this );
	m_motor0_up_tbtn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0UpBtnClick ), NULL, this );
	m_motor0_stop_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0Stop ), NULL, this );
	m_motor1_pos_text->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor1SetPos ), NULL, this );
	m_motor1_goto_text->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor1GoToPos ), NULL, this );
	m_motor1_home_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1Home ), NULL, this );
	m_motor1_down_tbtn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1DownBtnClick ), NULL, this );
	m_motor1_up_tbtn->Connect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1UpBtnClick ), NULL, this );
	m_motor1_stop_btn->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1Stop ), NULL, this );
	m_camera_settings_save->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCameraSettingsSave ), NULL, this );
	m_camera_action_btn1->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnSaveFrameAsRGB8bitmap ), NULL, this );
	m_camera_action_btn11->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnSaveFrameAsMonoRAW16 ), NULL, this );
}

MainWindow::~MainWindow()
{
	// Disconnect Events
	this->Disconnect( m_service_timer.GetId(), wxEVT_TIMER, wxTimerEventHandler( MainWindow::OnServiceTimer ) );
	this->Disconnect( m_controller_timer.GetId(), wxEVT_TIMER, wxTimerEventHandler( MainWindow::OnControllerTimer ) );
	this->Disconnect( m_camera_timer.GetId(), wxEVT_TIMER, wxTimerEventHandler( MainWindow::OnCameraTimer ) );
	m_button12->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnDCMFileOpen ), NULL, this );
	m_current_frame->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnCurrentFrameChange ), NULL, this );
	m_button303->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCurrentFramePrev ), NULL, this );
	m_button3012->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCurrentFrameNext ), NULL, this );
	m_frame_window_level->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnWindowLevelChange ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_TOP, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_BOTTOM, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_LINEUP, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_PAGEUP, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_window_level_scroll_bar->Disconnect( wxEVT_SCROLL_CHANGED, wxScrollEventHandler( MainWindow::OnWindowLevelScroll ), NULL, this );
	m_frame_window_width->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnWindowWidthChange ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_TOP, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_BOTTOM, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_LINEUP, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_LINEDOWN, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_PAGEUP, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_PAGEDOWN, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_THUMBRELEASE, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_window_width_scroll_bar->Disconnect( wxEVT_SCROLL_CHANGED, wxScrollEventHandler( MainWindow::OnWindowWidthScroll ), NULL, this );
	m_rotation90Left_btn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate90Left ), NULL, this );
	m_rotation0_btn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate0 ), NULL, this );
	m_rotation90Right_btn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate90Right ), NULL, this );
	m_rotation180_btn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameRotate180 ), NULL, this );
	m_flipH_btn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameFlipHorizontally ), NULL, this );
	m_flipV_btn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnFrameFlipVertically ), NULL, this );
	m_checkBox_light_only_when_capture->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( MainWindow::OnLightOnlyWhenCapture ), NULL, this );
	m_start_scan_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnScanStart ), NULL, this );
	m_stop_scan_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnScanStop ), NULL, this );
	m_tabs->Disconnect( wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler( MainWindow::OnTabChange ), NULL, this );
	m_button1->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnJPGToDCMConvert ), NULL, this );
	m_button11->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnBINToDCMConvert ), NULL, this );
	m_button111->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnDCMCreate ), NULL, this );
	m_controller_settings_save->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerSettingsSave ), NULL, this );
	m_controller_init_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerInitBtn ), NULL, this );
	m_controller_shell_clear_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerShellClear ), NULL, this );
	m_controller_shell_cmd_text->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnControllerShellCmdInput ), NULL, this );
	m_controller_shell_send_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnControllerShellCmdInput ), NULL, this );
	m_output0_check->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( MainWindow::OnOutput0Check ), NULL, this );
	m_output1_check->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( MainWindow::OnOutput1Check ), NULL, this );
	m_calibrate_camera_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCalibrateCamera ), NULL, this );
	m_calculate_offsets_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCalculateOffset ), NULL, this );
	m_motor0_pos_text->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor0SetPos ), NULL, this );
	m_motor0_goto_text->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor0GoToPos ), NULL, this );
	m_motor0_home_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0Home ), NULL, this );
	m_motor0_down_tbtn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0DownBtnClick ), NULL, this );
	m_motor0_up_tbtn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0UpBtnClick ), NULL, this );
	m_motor0_stop_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor0Stop ), NULL, this );
	m_motor1_pos_text->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor1SetPos ), NULL, this );
	m_motor1_goto_text->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( MainWindow::OnMotor1GoToPos ), NULL, this );
	m_motor1_home_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1Home ), NULL, this );
	m_motor1_down_tbtn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1DownBtnClick ), NULL, this );
	m_motor1_up_tbtn->Disconnect( wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1UpBtnClick ), NULL, this );
	m_motor1_stop_btn->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnMotor1Stop ), NULL, this );
	m_camera_settings_save->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnCameraSettingsSave ), NULL, this );
	m_camera_action_btn1->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnSaveFrameAsRGB8bitmap ), NULL, this );
	m_camera_action_btn11->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( MainWindow::OnSaveFrameAsMonoRAW16 ), NULL, this );

}

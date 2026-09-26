// for compilers that support precompilation, includes "wx/wx.h"
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <tape_scanner/utils/Logger.h>
#include "MyMainWindow.hpp"

class MyApp : public wxApp
{
public:
  virtual bool OnInit();
};

bool MyApp::OnInit()
{
  get_logger() << "OnInit() start \n";

  wxLog::SetActiveTarget( new wxLogStream());
  wxLog::SetTimestamp("[%H:%M:%S,%l]");

  MyMainWindow *main_window = new MyMainWindow((wxWindow*)NULL);
  wxIcon icon (wxT("main.ico"), wxBITMAP_TYPE_ICO);
  main_window->SetIcon(icon);
  main_window->Show(true);
  main_window->ShowFullScreen(1);
  main_window->ShowFullScreen(0);
  get_logger() << "OnInit() stop \n";
  return true;
}

wxIMPLEMENT_APP(MyApp);

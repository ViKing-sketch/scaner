#pragma once

#include <tape_scanner/scanner/NetworkSetup.hpp>

#include <wx/wx.h>

#include <functional>

class NetworkSettingsDialog : public wxDialog {
public:
  struct Values {
    wxString controller_ip;
    wxString controller_mask;
    wxString controller_gateway;
    wxString controller_port;
    wxString camera_ip;
    wxString camera_mask;
    wxString camera_gateway;
    bool camera_dhcp = false;
  };

  NetworkSettingsDialog(wxWindow *parent, bool russian, std::function<wxString(const wxString &)> translate,
                        const Values &current);

  void SetCameraSearch(std::function<tape_scanner::CameraSearchResult(const std::string &hint_ip)> search);
  void SetControllerProbe(std::function<bool(const std::string &ip, int port)> probe);

  Values GetValues() const;

private:
  wxString Tr(const wxString &text) const;
  static std::string ToStd(const wxString &text);
  static wxString Trimmed(const wxTextCtrl *ctrl);
  bool ParseController(std::string &ip, int &port, std::string &error) const;
  bool ParseAll(std::string &error) const;
  void ShowResult(const std::string &text, bool alert);

  void OnFind(wxCommandEvent &event);
  void OnCheck(wxCommandEvent &event);
  void OnSave(wxCommandEvent &event);
  void OnCancel(wxCommandEvent &event);

  bool m_russian = false;
  std::function<wxString(const wxString &)> m_translate;
  std::function<tape_scanner::CameraSearchResult(const std::string &)> m_search;
  std::function<bool(const std::string &, int)> m_probe;

  wxTextCtrl *m_controller_ip = nullptr;
  wxTextCtrl *m_controller_mask = nullptr;
  wxTextCtrl *m_controller_gateway = nullptr;
  wxTextCtrl *m_controller_port = nullptr;
  wxTextCtrl *m_camera_ip = nullptr;
  wxTextCtrl *m_camera_mask = nullptr;
  wxTextCtrl *m_camera_gateway = nullptr;
  wxCheckBox *m_camera_dhcp = nullptr;
  wxTextCtrl *m_result = nullptr;
};

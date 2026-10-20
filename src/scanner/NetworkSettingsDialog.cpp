#include <tape_scanner/scanner/NetworkSettingsDialog.hpp>

namespace {

constexpr int kFieldWidth = 140;

void AddRow(wxFlexGridSizer *grid, wxWindow *parent, const wxString &label, wxWindow *field) {
  auto *text = new wxStaticText(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
  grid->Add(text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
  grid->Add(field, 0, wxALL | wxEXPAND, 4);
}

} // namespace

NetworkSettingsDialog::NetworkSettingsDialog(wxWindow *parent, bool russian,
                                             std::function<wxString(const wxString &)> translate, const Values &current)
    : wxDialog(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_russian(russian), m_translate(std::move(translate)) {
  SetTitle(Tr("Network settings"));

  auto *root = new wxBoxSizer(wxVERTICAL);

  auto *note = new wxStaticText(this, wxID_ANY, wxString::FromUTF8(tape_scanner::SubnetInstruction(m_russian)));
  note->Wrap(480);
  root->Add(note, 0, wxALL | wxEXPAND, 10);

  auto *controller_frame = new wxStaticBox(this, wxID_ANY, Tr("Controller"));
  auto *controller_box = new wxStaticBoxSizer(controller_frame, wxVERTICAL);
  auto *controller_grid = new wxFlexGridSizer(0, 2, 0, 0);
  controller_grid->AddGrowableCol(1, 1);
  m_controller_ip =
      new wxTextCtrl(controller_frame, wxID_ANY, current.controller_ip, wxDefaultPosition, wxSize(kFieldWidth, -1));
  m_controller_mask =
      new wxTextCtrl(controller_frame, wxID_ANY, current.controller_mask, wxDefaultPosition, wxSize(kFieldWidth, -1));
  m_controller_gateway = new wxTextCtrl(controller_frame, wxID_ANY, current.controller_gateway, wxDefaultPosition,
                                        wxSize(kFieldWidth, -1));
  m_controller_port = new wxTextCtrl(controller_frame, wxID_ANY, current.controller_port, wxDefaultPosition, wxSize(80, -1));
  AddRow(controller_grid, controller_frame, Tr("IP address:"), m_controller_ip);
  AddRow(controller_grid, controller_frame, Tr("Network mask:"), m_controller_mask);
  AddRow(controller_grid, controller_frame, Tr("Gateway:"), m_controller_gateway);
  AddRow(controller_grid, controller_frame, Tr("UDP port:"), m_controller_port);
  controller_box->Add(controller_grid, 0, wxEXPAND);
  root->Add(controller_box, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);

  auto *camera_frame = new wxStaticBox(this, wxID_ANY, Tr("Camera"));
  auto *camera_box = new wxStaticBoxSizer(camera_frame, wxVERTICAL);
  auto *camera_grid = new wxFlexGridSizer(0, 2, 0, 0);
  camera_grid->AddGrowableCol(1, 1);
  m_camera_ip = new wxTextCtrl(camera_frame, wxID_ANY, current.camera_ip, wxDefaultPosition, wxSize(kFieldWidth, -1));
  m_camera_mask =
      new wxTextCtrl(camera_frame, wxID_ANY, current.camera_mask, wxDefaultPosition, wxSize(kFieldWidth, -1));
  m_camera_gateway =
      new wxTextCtrl(camera_frame, wxID_ANY, current.camera_gateway, wxDefaultPosition, wxSize(kFieldWidth, -1));
  m_camera_dhcp = new wxCheckBox(camera_frame, wxID_ANY, Tr("Use DHCP client"));
  m_camera_dhcp->SetValue(current.camera_dhcp);
  AddRow(camera_grid, camera_frame, Tr("IP address:"), m_camera_ip);
  AddRow(camera_grid, camera_frame, Tr("Network mask:"), m_camera_mask);
  AddRow(camera_grid, camera_frame, Tr("Gateway:"), m_camera_gateway);
  AddRow(camera_grid, camera_frame, Tr("Dynamic IP:"), m_camera_dhcp);
  camera_box->Add(camera_grid, 0, wxEXPAND);
  root->Add(camera_box, 0, wxALL | wxEXPAND, 10);

  m_result = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 110),
                            wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
  root->Add(m_result, 1, wxLEFT | wxRIGHT | wxEXPAND, 10);

  auto *buttons = new wxBoxSizer(wxHORIZONTAL);
  auto *find = new wxButton(this, wxID_ANY, Tr("Find camera"));
  auto *check = new wxButton(this, wxID_ANY, Tr("Check controller"));
  auto *save = new wxButton(this, wxID_ANY, Tr("Save"));
  auto *cancel = new wxButton(this, wxID_CANCEL, Tr("Cancel"));
  find->Bind(wxEVT_BUTTON, &NetworkSettingsDialog::OnFind, this);
  check->Bind(wxEVT_BUTTON, &NetworkSettingsDialog::OnCheck, this);
  save->Bind(wxEVT_BUTTON, &NetworkSettingsDialog::OnSave, this);
  cancel->Bind(wxEVT_BUTTON, &NetworkSettingsDialog::OnCancel, this);
  buttons->Add(find, 0, wxALL, 5);
  buttons->Add(check, 0, wxALL, 5);
  buttons->AddStretchSpacer(1);
  save->SetDefault();
  buttons->Add(save, 0, wxALL, 5);
  buttons->Add(cancel, 0, wxALL, 5);
  root->Add(buttons, 0, wxEXPAND | wxALL, 5);

  SetEscapeId(wxID_CANCEL);
  SetSizerAndFit(root);
  SetMinSize(wxSize(520, 520));
  SetSize(wxSize(560, 620));
  Centre(wxBOTH);
}

void NetworkSettingsDialog::SetCameraSearch(
    std::function<tape_scanner::CameraSearchResult(const std::string &hint_ip)> search) {
  m_search = std::move(search);
}

void NetworkSettingsDialog::SetControllerProbe(std::function<bool(const std::string &ip, int port)> probe) {
  m_probe = std::move(probe);
}

NetworkSettingsDialog::Values NetworkSettingsDialog::GetValues() const {
  Values values;
  values.controller_ip = Trimmed(m_controller_ip);
  values.controller_mask = Trimmed(m_controller_mask);
  values.controller_gateway = Trimmed(m_controller_gateway);
  values.controller_port = Trimmed(m_controller_port);
  values.camera_ip = Trimmed(m_camera_ip);
  values.camera_mask = Trimmed(m_camera_mask);
  values.camera_gateway = Trimmed(m_camera_gateway);
  values.camera_dhcp = m_camera_dhcp->GetValue();
  return values;
}

wxString NetworkSettingsDialog::Tr(const wxString &text) const {
  return m_translate ? m_translate(text) : text;
}

std::string NetworkSettingsDialog::ToStd(const wxString &text) {
  wxString copy = text;
  copy.Trim().Trim(false);
  const auto buf = copy.ToUTF8();
  if (!buf)
    return {};
  return std::string(buf.data(), buf.length());
}

wxString NetworkSettingsDialog::Trimmed(const wxTextCtrl *ctrl) {
  wxString value = ctrl->GetValue();
  value.Trim().Trim(false);
  return value;
}

bool NetworkSettingsDialog::ParseController(std::string &ip, int &port, std::string &error) const {
  ip = ToStd(m_controller_ip->GetValue());
  tape_scanner::Ipv4 parsed;
  if (!tape_scanner::ParseIpv4(ip, parsed)) {
    error = tape_scanner::InvalidIpMessage(m_russian);
    return false;
  }
  if (!tape_scanner::ParsePort(ToStd(m_controller_port->GetValue()), port)) {
    error = tape_scanner::InvalidPortMessage(m_russian);
    return false;
  }
  return true;
}

bool NetworkSettingsDialog::ParseAll(std::string &error) const {
  int port = 0;
  std::string ip;
  if (!ParseController(ip, port, error))
    return false;
  const wxTextCtrl *fields[] = {m_controller_mask, m_controller_gateway, m_camera_ip, m_camera_mask, m_camera_gateway};
  for (const wxTextCtrl *field : fields) {
    tape_scanner::Ipv4 parsed;
    if (!tape_scanner::ParseIpv4(ToStd(field->GetValue()), parsed)) {
      error = tape_scanner::InvalidIpMessage(m_russian);
      return false;
    }
  }
  return true;
}

void NetworkSettingsDialog::ShowResult(const std::string &text, bool alert) {
  const wxString message = wxString::FromUTF8(text);
  m_result->SetValue(message);
  if (alert)
    wxMessageBox(message, GetTitle(), wxOK | wxICON_ERROR, this);
}

void NetworkSettingsDialog::OnFind(wxCommandEvent &) {
  const std::string ip = ToStd(m_camera_ip->GetValue());
  tape_scanner::Ipv4 parsed;
  if (!tape_scanner::ParseIpv4(ip, parsed)) {
    ShowResult(tape_scanner::InvalidIpMessage(m_russian), true);
    return;
  }
  if (!m_search) {
    ShowResult(tape_scanner::CameraSdkFailedMessage(m_russian), true);
    return;
  }
  const tape_scanner::CameraSearchResult result = m_search(ip);
  const bool failed = !result.sdk_ok || result.cameras.empty();
  ShowResult(tape_scanner::FormatCameraSearch(result, m_russian), failed);
}

void NetworkSettingsDialog::OnCheck(wxCommandEvent &) {
  std::string ip;
  int port = 0;
  std::string error;
  if (!ParseController(ip, port, error)) {
    ShowResult(error, true);
    return;
  }
  if (!m_probe) {
    ShowResult(tape_scanner::ControllerSilentMessage(ip, port, m_russian), true);
    return;
  }
  const bool answered = m_probe(ip, port);
  const std::string message = answered ? tape_scanner::ControllerAnsweredMessage(m_russian)
                                       : tape_scanner::ControllerSilentMessage(ip, port, m_russian);
  ShowResult(message, !answered);
  if (answered)
    wxMessageBox(wxString::FromUTF8(message), GetTitle(), wxOK | wxICON_INFORMATION, this);
}

void NetworkSettingsDialog::OnSave(wxCommandEvent &) {
  std::string error;
  if (!ParseAll(error)) {
    ShowResult(error, true);
    return;
  }
  EndModal(wxID_OK);
}

void NetworkSettingsDialog::OnCancel(wxCommandEvent &) { EndModal(wxID_CANCEL); }

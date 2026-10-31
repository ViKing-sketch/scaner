#include <tape_scanner/scanner/Controller.hpp>

#include <wx/log.h>

#include <cstdio>


namespace {

constexpr int DEFAULT_TIMEOUT_US = 100;
constexpr int DEFAULT_TIMEOUT_SEC = 0;

/// Четыре октета и ничего больше. Иначе Hostname() уйдёт в DNS.
bool IsDottedIPv4(const wxString &text) {
    const std::string ip = text.ToStdString();
    unsigned a = 0;
    unsigned b = 0;
    unsigned c = 0;
    unsigned d = 0;
    char extra = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) {
        return false;
    }
    return a <= 255 && b <= 255 && c <= 255 && d <= 255;
}

}

Controller::Controller(wxString ip_address, int port) {
    // wx 3.2: IPAddress() только читает адрес. Hostname() для dotted quad
    // берёт inet_addr и DNS не вызывает. Чужую строку в Hostname не отдаём.
    if (port < 0 || port > 65535 || !IsDottedIPv4(ip_address) || !m_address.Hostname(ip_address) ||
        !m_address.Service(static_cast<unsigned short>(port))) {
        m_bad_address = true;
        return;
    }
    InitSocket();
}

bool Controller::TryConnect() {
    if (m_bad_address) {
        m_status = Status::disconnected;
        return false;
    }
    m_status = Status::try_connect;
    const bool res = OpenConnection();
    if (res) {
        m_status = Status::connected;
        InitControllerAfterConnect();
    } else {
        m_status = Status::disconnected;
    }
    return res;
}

bool Controller::IsConnected() {
    return m_status == Status::connected;
}

void Controller::SendAnyCommand(const std::string& any_command) {
    SendData(any_command);
}

void Controller::GoCameraTo(int x, bool wait) {
    SetSpeed();
    // Temporary workaround: controller ignores odd coordinates.
    if (x % 2)
        ++x;

    auto done = SendCommand("G0 X" + std::to_string(x), wait ? 2 : 0, wait ? DEFAULT_TIMEOUT_US : 0);

    if (wait && done != "DONE") {
        WaitForCameroGoTo(x);
    }
}

void Controller::GoCameraToHome(bool wait) {
    SetSpeed();
    SendCommand("G0 HOME", wait ? 2 : 0, wait ? DEFAULT_TIMEOUT_US : 0);
    if (wait) {
        WaitForCameroGoTo(0);
    }
}

void Controller::WaitForCameroGoTo(int x) {
    while (SendCommand("G0 POSITION", 1) != std::to_string(x)) {
    }
}

void Controller::WaitForRollerGoTo(int x) {
  while (SendCommand("G1 POSITION", 1) != std::to_string(x)) {
  }
}

void Controller::SetSpeed(int speed) {
    if (speed)
        m_cur_camera_speed = speed;

    SendCommand("G0 F" + std::to_string(m_cur_camera_speed), DEFAULT_TIMEOUT_SEC, DEFAULT_TIMEOUT_US);
}

void Controller::SetAcceleration(int acceleration) {
    SendCommand("G0 A" + std::to_string(acceleration), DEFAULT_TIMEOUT_SEC, DEFAULT_TIMEOUT_US);
}

void Controller::SetRollerSpeed(int speed) {
    if (speed)
        m_cur_roller_speed = speed;
    SendCommand("G1 F" + std::to_string(m_cur_roller_speed), DEFAULT_TIMEOUT_SEC, DEFAULT_TIMEOUT_US);
}

void Controller::StopCameraMotor() {
    SendData("G0 STOP");
}

void Controller::StartRollers() {
    SendData("G1 X500000");
}

void Controller::StopRollers() {
    SendData("G1 STOP");
    ResetRollerPos();
}

void Controller::ResetRollerPos() {
    SendCommand("G1 HOME");
}

void Controller::GoRollersTo(int x, bool wait) {
    SetRollerSpeed();
    if (x % 2)
        ++x;

    SendCommand("G1 X" + std::to_string(x), wait ? 1 : 0);
    if (wait) {
      WaitForRollerGoTo(x);
    }
}

void Controller::LightEnable(bool enable) {
    SendCommand(std::string("LIGHT ") + (enable ? "ON" : "OFF"), 0, 100);
}

bool Controller::HasFilm() {
    SendData("FILM");
    auto response = ReadData(5);
    return response == "FILM DETECTED";
}

void Controller::InitSocket() {
    // Блокирующий сокет на Windows не возвращается из connect(), пока не истечёт TCP-таймаут (~20 с).
    m_socket.SetFlags(wxSOCKET_NOWAIT);
    m_socket.SetTimeout(1);
    m_in_buf.resize(in_buf_size);
}

bool Controller::OpenConnection() {
  if (m_bad_address) {
    return false;
  }
  m_socket.Close();
  m_socket.SetFlags(wxSOCKET_NOWAIT);
  m_socket.SetTimeout(1);
  m_socket.Connect(m_address, false);
  const bool finished = m_socket.WaitOnConnect(0, 300);
  if (!finished || !m_socket.IsConnected()) {
    m_socket.Close();
    return false;
  }
  m_socket.SetFlags(wxSOCKET_WAITALL_WRITE);
  return true;
}

void Controller::InitControllerAfterConnect() {
    SetAcceleration(100000);
    SetSpeed(15000);
    SetRollerSpeed(15000);
    SendCommand("G1 DIR1", 1);
    ResetRollerPos();
    // GoCameraToHome(false);
}

void Controller::SendData(const std::string& data) {
    if (m_status != Status::connected)
        throw std::logic_error("Camera motor is BUSY or connection status not connected");

    ClearInput();
    if(auto ready = m_socket.WaitForWrite(5)) {
      m_socket.Write(data.data(), data.size());
      wxLogMessage("Controller: data writen: %s", data);
    }
    else{
      wxLogError("Controller: send data failed: %s", data);
    }
}

std::string Controller::SendCommand(const std::string& data, int wait_sec, int wait_usec) {
    SendData(data);
    auto response = (wait_sec + wait_usec > 0) ? ReadData(wait_sec, wait_usec) : "";
    return response;
}

std::string& Controller::ReadData(int wait_sec, int wait_usec, size_t expected_count) {
    if (wait_sec || wait_usec) {
        m_socket.WaitForRead(wait_sec, wait_usec);
    }

    if (!m_socket.IsData()) {
        wxLogMessage("No data to read");
        m_in_buf = "";
        return m_in_buf;
    }

    m_in_buf.resize(expected_count);
    m_socket.Read(m_in_buf.data(), m_in_buf.size());
    m_in_buf.resize(m_socket.LastReadCount());
    wxLogMessage("Read data: %s", m_in_buf);
    return m_in_buf;
}

void Controller::ClearInput() {
    while (m_socket.IsData()) {
        std::vector<int8_t> tmp(1024);
        m_socket.Read(tmp.data(), tmp.size());
    }
}

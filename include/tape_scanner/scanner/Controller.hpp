#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <wx/event.h>
#include <wx/socket.h>

wxDECLARE_EVENT(CONTROLLER_EVENT, wxCommandEvent);

class Controller : public wxEvtHandler {
public:
    enum class UserEvtType {
        CONNECTED,
        DISCONNECTED,
        TRY_CONNECT_FAILED,
        UNKNOWN
    };

    Controller(wxString ip_address, int port);
    ~Controller() override = default;

    bool TryConnect();
    bool IsConnected();
    // TCP handshake only. Does not send motion commands and does not keep the socket.
    bool Probe();

    void SendAnyCommand(const std::string& any_command);
    void GoCameraTo(int x, bool wait = true);
    void GoCameraToHome(bool wait = true);
    void WaitForCameroGoTo(int x);
    void WaitForRollerGoTo(int x);
    void SetSpeed(int speed = 0);
    void SetAcceleration(int acceleration);
    void SetRollerSpeed(int speed = 0);
    void StopCameraMotor();
    void StartRollers();
    void StopRollers();
    void ResetRollerPos();
    void GoRollersTo(int x, bool wait = false);
    void LightEnable(bool enable);
    bool HasFilm();

private:
    void InitSocket();
    bool OpenConnection();
    void InitControllerAfterConnect();
    void SendData(const std::string& data);
    std::string SendCommand(const std::string& data, int wait_sec = 1, int wait_usec = 0);
    std::string& ReadData(int wait_sec = 0, int wait_usec = 0, size_t expected_count = in_buf_size);
    void ClearInput();

    static inline constexpr size_t in_buf_size = 1024;

    wxIPV4address m_address;
    wxSocketClient m_socket;
    enum class Status { try_connect, connected, disconnected };
    Status m_status = Status::disconnected;
    int m_cur_camera_speed = 30000;
    int m_cur_camera_acceleration = 10000;
    int m_cur_roller_speed = 14000;
    int m_cur_roller_acceleration = 10000;
    std::string m_in_buf;
    int m_has_film_status = -1;
};

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include <tape_scanner/scanner/LinkLoss.hpp>

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
    /// То же соединение, что и TryConnect, но без G1 HOME. Для обрыва связи.
    bool Reconnect();
    /// Сокет жив и команда положения ушла. Обрыв помечает соединение закрытым.
    bool Probe();
    bool IsConnected();
    /// G0 STOP и G1 STOP. Без home и без сброса позиции.
    bool HaltMotors();
    /// Сначала стоп, затем G0 HOME и G1 HOME. Только если так сказано в settings.ini.
    bool HomeMotors();

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
    bool OpenAndInit(bool startup_home);
    void MarkDisconnected();
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

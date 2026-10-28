#pragma once

#include <functional>

// Непрерывный опрос связи с контроллером.
// Проба — существующая проверка сокета (TryConnect / IsConnected) или симулятор.
// Командный TCP-протокол контроллера здесь не реализуется: Z04 ждёт Макса.

inline constexpr const char *kControllerConnectedStatus = "Controller connected";
inline constexpr const char *kControllerDisconnectedStatus = "Controller disconnected";

enum class ControllerLinkState {
    Disconnected,
    Connected,
};

struct ControllerLinkSnapshot {
    ControllerLinkState state = ControllerLinkState::Disconnected;
    bool scan_enabled = false;
    const char *status_text = kControllerDisconnectedStatus;
};

class ControllerLinkPoll {
public:
    using Probe = std::function<bool()>;

    explicit ControllerLinkPoll(Probe probe = {});

    /// Один тик таймера. Проба вызывается каждый раз: и когда связь есть, и когда её нет.
    ControllerLinkSnapshot Poll();

    ControllerLinkSnapshot Current() const { return m_current; }

    int PollCount() const { return m_polls; }

private:
    Probe m_probe;
    ControllerLinkSnapshot m_current{};
    int m_polls = 0;
};

ControllerLinkSnapshot SnapshotFromControllerLink(bool up);

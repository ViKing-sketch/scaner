#include <tape_scanner/scanner/ControllerLink.hpp>

#include <utility>

ControllerLinkSnapshot SnapshotFromControllerLink(bool up) {
    ControllerLinkSnapshot snap;
    if (!up) {
        return snap;
    }
    snap.state = ControllerLinkState::Connected;
    snap.scan_enabled = true;
    snap.status_text = kControllerConnectedStatus;
    return snap;
}

ControllerLinkPoll::ControllerLinkPoll(Probe probe) : m_probe(std::move(probe)) {}

ControllerLinkSnapshot ControllerLinkPoll::Poll() {
    ++m_polls;
    bool up = false;
    if (m_probe) {
        try {
            up = static_cast<bool>(m_probe());
        } catch (...) {
            up = false;
        }
    }
    m_current = SnapshotFromControllerLink(up);
    return m_current;
}

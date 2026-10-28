#include <tape_scanner/scanner/ControllerLink.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool ok, const char *expr, int line) {
    if (!ok) {
        std::cerr << "FAIL line " << line << ": " << expr << "\n";
        ++g_failures;
    }
    ++g_checks;
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

void TestDisconnectedDisablesScan() {
    bool link = false;
    int calls = 0;
    ControllerLinkPoll poll([&] {
        ++calls;
        return link;
    });

    const ControllerLinkSnapshot before = poll.Current();
    CHECK(before.state == ControllerLinkState::Disconnected);
    CHECK(!before.scan_enabled);
    CHECK(std::string(before.status_text) == kControllerDisconnectedStatus);
    CHECK(poll.PollCount() == 0);

    const ControllerLinkSnapshot down = poll.Poll();
    CHECK(down.state == ControllerLinkState::Disconnected);
    CHECK(!down.scan_enabled);
    CHECK(std::string(down.status_text) == kControllerDisconnectedStatus);
    CHECK(calls == 1);

    const ControllerLinkSnapshot still_down = poll.Poll();
    CHECK(!still_down.scan_enabled);
    CHECK(std::string(still_down.status_text) == kControllerDisconnectedStatus);
    CHECK(calls == 2);
    CHECK(poll.PollCount() == 2);
    CHECK(!poll.Current().scan_enabled);
}

void TestConnectedAllowsScan() {
    bool link = true;
    ControllerLinkPoll poll([&] { return link; });

    const ControllerLinkSnapshot up = poll.Poll();
    CHECK(up.state == ControllerLinkState::Connected);
    CHECK(up.scan_enabled);
    CHECK(std::string(up.status_text) == kControllerConnectedStatus);
    CHECK(poll.Current().scan_enabled);

    link = false;
    const ControllerLinkSnapshot dropped = poll.Poll();
    CHECK(dropped.state == ControllerLinkState::Disconnected);
    CHECK(!dropped.scan_enabled);
    CHECK(std::string(dropped.status_text) == kControllerDisconnectedStatus);

    link = true;
    const ControllerLinkSnapshot restored = poll.Poll();
    CHECK(restored.scan_enabled);
    CHECK(std::string(restored.status_text) == kControllerConnectedStatus);
    CHECK(poll.PollCount() == 3);
}

void TestProbeFailureStaysDisabled() {
    ControllerLinkPoll missing;
    const ControllerLinkSnapshot none = missing.Poll();
    CHECK(!none.scan_enabled);
    CHECK(std::string(none.status_text) == kControllerDisconnectedStatus);

    ControllerLinkPoll broken([]() -> bool { throw std::runtime_error("link down"); });
    const ControllerLinkSnapshot failed = broken.Poll();
    CHECK(failed.state == ControllerLinkState::Disconnected);
    CHECK(!failed.scan_enabled);
    CHECK(std::string(failed.status_text) == kControllerDisconnectedStatus);
    CHECK(broken.PollCount() == 1);
}

} // namespace

int main() {
    // Камера, wx и сокет контроллера не нужны: проба подменяется.
    TestDisconnectedDisablesScan();
    TestConnectedAllowsScan();
    TestProbeFailureStaysDisabled();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << g_checks << " controller-link checks passed\n";
    return 0;
}

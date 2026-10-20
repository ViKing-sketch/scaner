#include <tape_scanner/scanner/NetworkSetup.hpp>

#include <iostream>
#include <string>

namespace {

int g_failures = 0;

void Check(bool ok, const char *expr, int line) {
  if (!ok) {
    std::cerr << "FAIL line " << line << ": " << expr << "\n";
    ++g_failures;
  }
}

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

} // namespace

int main() {
  tape_scanner::Ipv4 ip;
  CHECK(tape_scanner::ParseIpv4("192.168.1.10", ip));
  CHECK(ip.a == 192 && ip.b == 168 && ip.c == 1 && ip.d == 10);
  CHECK(tape_scanner::ParseIpv4("  192.168.1.243  ", ip));
  CHECK(ip.d == 243);
  CHECK(tape_scanner::ParseIpv4("0.0.0.0", ip));
  CHECK(tape_scanner::ParseIpv4("255.255.255.255", ip));
  CHECK(tape_scanner::ParseIpv4("192.168.001.010", ip));
  CHECK(ip.c == 1 && ip.d == 10);
  CHECK(!tape_scanner::ParseIpv4("", ip));
  CHECK(!tape_scanner::ParseIpv4("192.168.1", ip));
  CHECK(!tape_scanner::ParseIpv4("192.168.1.256", ip));
  CHECK(!tape_scanner::ParseIpv4("192.168.1.1.1", ip));
  CHECK(!tape_scanner::ParseIpv4("192.168.1.", ip));
  CHECK(!tape_scanner::ParseIpv4("abc", ip));
  CHECK(tape_scanner::ParseIpv4("192.168.1.10 ", ip));

  int port = 0;
  CHECK(tape_scanner::ParsePort("1000", port) && port == 1000);
  CHECK(tape_scanner::ParsePort("1", port) && port == 1);
  CHECK(tape_scanner::ParsePort("65535", port) && port == 65535);
  CHECK(tape_scanner::ParsePort(" 4196", port) && port == 4196);
  CHECK(!tape_scanner::ParsePort("0", port));
  CHECK(!tape_scanner::ParsePort("65536", port));
  CHECK(!tape_scanner::ParsePort("", port));
  CHECK(!tape_scanner::ParsePort("-1", port));
  CHECK(!tape_scanner::ParsePort("10a", port));

  const std::string silent_ru = tape_scanner::ControllerSilentMessage("192.168.1.10", 1000, true);
  CHECK(silent_ru.find("192.168.1.10:1000") != std::string::npos);
  CHECK(silent_ru.find("не отвечает") != std::string::npos);
  CHECK(silent_ru.find("одной подсети") != std::string::npos);
  const std::string silent_en = tape_scanner::ControllerSilentMessage("192.168.1.10", 1000, false);
  CHECK(silent_en.find("did not answer") != std::string::npos);
  CHECK(silent_en.find("192.168.1.10:1000") != std::string::npos);

  const std::string subnet = tape_scanner::SubnetInstruction(true);
  CHECK(subnet == "Компьютер сканера и контроллер должны быть в своей подсети, отдельно от офисной сети.");
  CHECK(tape_scanner::SubnetInstruction(false).find("own subnet") != std::string::npos);

  tape_scanner::CameraSearchResult empty;
  empty.sdk_ok = true;
  CHECK(tape_scanner::FormatCameraSearch(empty, true).find("Камера не найдена") != std::string::npos);

  tape_scanner::CameraSearchResult failed;
  failed.sdk_ok = false;
  failed.sdk_error = "CameraSdkInit failed";
  CHECK(tape_scanner::FormatCameraSearch(failed, true).find("SDK") != std::string::npos);

  tape_scanner::CameraSearchResult found;
  found.sdk_ok = true;
  found.cameras.push_back(tape_scanner::FoundCamera{"MV-GE", "SN1", "GigE", "192.168.1.243", "255.255.255.0", "192.168.1.1"});
  const std::string listed = tape_scanner::FormatCameraSearch(found, true);
  CHECK(listed.find("Найденные камеры:") != std::string::npos);
  CHECK(listed.find("MV-GE") != std::string::npos);
  CHECK(listed.find("SN1") != std::string::npos);
  CHECK(listed.find("192.168.1.243") != std::string::npos);

  CHECK(tape_scanner::ControllerAnsweredMessage(true) == "Контроллер ответил.");
  CHECK(tape_scanner::InvalidIpMessage(false) == "Invalid IP address.");
  CHECK(tape_scanner::InvalidPortMessage(true).find("UDP") != std::string::npos);

  if (g_failures) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "ok\n";
  return 0;
}

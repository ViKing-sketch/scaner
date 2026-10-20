#include <tape_scanner/scanner/NetworkSetup.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace tape_scanner {
namespace {

std::string Trim(std::string text) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
  text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
  return text;
}

std::string Pick(bool russian, const char *ru, const char *en) { return russian ? ru : en; }

} // namespace

bool ParseIpv4(const std::string &text, Ipv4 &out) {
  const std::string trimmed = Trim(text);
  int parts[4] = {};
  std::stringstream stream(trimmed);
  std::string item;
  int count = 0;
  while (std::getline(stream, item, '.')) {
    if (count >= 4 || item.empty() || item.size() > 3)
      return false;
    for (unsigned char c : item) {
      if (!std::isdigit(c))
        return false;
    }
    const int value = std::stoi(item);
    if (value < 0 || value > 255)
      return false;
    parts[count++] = value;
  }
  if (count != 4 || stream.bad())
    return false;
  out = Ipv4{parts[0], parts[1], parts[2], parts[3]};
  return true;
}

bool ParsePort(const std::string &text, int &port) {
  const std::string trimmed = Trim(text);
  if (trimmed.empty() || trimmed.size() > 5)
    return false;
  for (unsigned char c : trimmed) {
    if (!std::isdigit(c))
      return false;
  }
  const int value = std::stoi(trimmed);
  if (value < 1 || value > 65535)
    return false;
  port = value;
  return true;
}

std::string ControllerSilentMessage(const std::string &ip, int port, bool russian) {
  const std::string where = ip + ":" + std::to_string(port);
  if (russian) {
    return "Контроллер " + where +
           " не отвечает. Проверьте кабель и что компьютер сканера в одной подсети с контроллером.";
  }
  return "Controller " + where +
         " did not answer. Check the cable and that the scanner PC is on the same subnet as the controller.";
}

std::string SubnetInstruction(bool russian) {
  // The Russian sentence is the opening line of docs/network.md.
  return Pick(russian,
              "Компьютер сканера и контроллер должны быть в своей подсети, отдельно от офисной сети.",
              "The scanner PC and the controller must be on their own subnet, separate from the office network.");
}

std::string CameraNotFoundMessage(bool russian) {
  return Pick(russian, "Камера не найдена. Проверьте кабель и IP-адрес камеры.",
              "No camera found. Check the cable and the camera IP address.");
}

std::string CameraSdkFailedMessage(bool russian) {
  return Pick(russian, "Не удалось запустить SDK камеры.", "Camera SDK failed to start.");
}

std::string ControllerAnsweredMessage(bool russian) {
  return Pick(russian, "Контроллер ответил.", "Controller answered.");
}

std::string InvalidIpMessage(bool russian) {
  return Pick(russian, "Неверный IP-адрес.", "Invalid IP address.");
}

std::string InvalidPortMessage(bool russian) {
  return Pick(russian, "Неверный UDP-порт.", "Invalid UDP port.");
}

std::string CamerasFoundHeading(bool russian) {
  return Pick(russian, "Найденные камеры:", "Cameras found:");
}

std::string ControllerAddressPendingMessage(bool russian) {
  return Pick(russian, "Новый адрес контроллера сохранён. Текущее соединение не разрывается.",
              "The new controller address is saved. The current connection is left unchanged.");
}

std::string FormatCameraSearch(const CameraSearchResult &result, bool russian) {
  if (!result.sdk_ok) {
    std::string text = CameraSdkFailedMessage(russian);
    if (!result.sdk_error.empty())
      text += " " + result.sdk_error;
    return text;
  }
  if (result.cameras.empty())
    return CameraNotFoundMessage(russian);

  std::string text = CamerasFoundHeading(russian);
  for (const FoundCamera &camera : result.cameras) {
    text += "\n";
    text += camera.name.empty() ? Pick(russian, "камера", "camera") : camera.name;
    if (!camera.serial.empty())
      text += ", " + camera.serial;
    if (!camera.ip.empty())
      text += ", " + camera.ip;
  }
  return text;
}

} // namespace tape_scanner

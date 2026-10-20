#pragma once

#include <string>
#include <vector>

namespace tape_scanner {

struct Ipv4 {
  int a = 0;
  int b = 0;
  int c = 0;
  int d = 0;
};

bool ParseIpv4(const std::string &text, Ipv4 &out);
bool ParsePort(const std::string &text, int &port);

std::string ControllerSilentMessage(const std::string &ip, int port, bool russian);
std::string SubnetInstruction(bool russian);
std::string CameraNotFoundMessage(bool russian);
std::string CameraSdkFailedMessage(bool russian);
std::string ControllerAnsweredMessage(bool russian);
std::string InvalidIpMessage(bool russian);
std::string InvalidPortMessage(bool russian);
std::string CamerasFoundHeading(bool russian);
std::string ControllerAddressPendingMessage(bool russian);

struct FoundCamera {
  std::string name;
  std::string serial;
  std::string port;
  std::string ip;
  std::string mask;
  std::string gateway;
};

struct CameraSearchResult {
  bool sdk_ok = false;
  std::string sdk_error;
  std::vector<FoundCamera> cameras;
};

std::string FormatCameraSearch(const CameraSearchResult &result, bool russian);

} // namespace tape_scanner

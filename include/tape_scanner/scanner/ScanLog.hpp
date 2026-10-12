#pragma once

#include <cstdio>
#include <string>

#ifdef TAPE_SCANNER_USE_WX
#include <wx/log.h>
#include <wx/string.h>
#endif

// Журнал ядра скана. В приложении с окном сообщения идут в wxLog,
// в юнит-тестах — в stdout/stderr, без wxWidgets и без сканера.

inline void ScanLogMessage(const std::string &text) {
#ifdef TAPE_SCANNER_USE_WX
  wxLogMessage("%s", wxString(text.c_str()));
#else
  std::fputs(text.c_str(), stdout);
  std::fputc('\n', stdout);
#endif
}

inline void ScanLogError(const std::string &text) {
#ifdef TAPE_SCANNER_USE_WX
  wxLogError("%s", wxString(text.c_str()));
#else
  std::fputs(text.c_str(), stderr);
  std::fputc('\n', stderr);
#endif
}

template <typename... Args>
inline void ScanLogMessageF(const char *fmt, Args... args) {
  char buf[1024];
  std::snprintf(buf, sizeof(buf), fmt, args...);
  ScanLogMessage(buf);
}

template <typename... Args>
inline void ScanLogErrorF(const char *fmt, Args... args) {
  char buf[1024];
  std::snprintf(buf, sizeof(buf), fmt, args...);
  ScanLogError(buf);
}

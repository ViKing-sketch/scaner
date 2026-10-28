#include <tape_scanner/scanner/FileCommit.hpp>

#include <cerrno>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

bool SameDirectory(const std::filesystem::path &temp_path, const std::filesystem::path &final_path) {
  return temp_path.has_parent_path() && temp_path.parent_path() == final_path.parent_path();
}

} // namespace

bool FsyncFile(const std::filesystem::path &path) {
#ifdef _WIN32
  HANDLE handle = CreateFileA(path.string().c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  const BOOL flushed = FlushFileBuffers(handle);
  CloseHandle(handle);
  return flushed != FALSE;
#else
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    return false;
  }
  const int rc = ::fsync(fd);
  ::close(fd);
  return rc == 0;
#endif
}

bool FsyncDirectory(const std::filesystem::path &directory) {
#ifdef _WIN32
  HANDLE handle = CreateFileA(directory.string().c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }
  const BOOL flushed = FlushFileBuffers(handle);
  CloseHandle(handle);
  return flushed != FALSE;
#else
  const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd < 0) {
    return false;
  }
  const int rc = ::fsync(fd);
  ::close(fd);
  return rc == 0;
#endif
}

bool PublishNewFile(const std::filesystem::path &temp_path, const std::filesystem::path &final_path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(temp_path, error) || error) {
    return false;
  }
  if (!SameDirectory(temp_path, final_path)) {
    return false;
  }
  if (std::filesystem::exists(final_path, error) || error) {
    return false;
  }
  if (!FsyncFile(temp_path)) {
    return false;
  }

#ifdef _WIN32
  if (!MoveFileExA(temp_path.string().c_str(), final_path.string().c_str(), MOVEFILE_WRITE_THROUGH)) {
    return false;
  }
#else
  if (::link(temp_path.c_str(), final_path.c_str()) != 0) {
    if (errno == EEXIST) {
      return false;
    }
    // Файловая система без жёстких ссылок. Имя публикуется через rename
    // только пока его ещё нет. Гонка двух процессов этим путём не закрыта.
    if (errno == EPERM || errno == EOPNOTSUPP || errno == ENOTSUP || errno == EXDEV) {
      if (std::filesystem::exists(final_path, error) || error) {
        return false;
      }
      error.clear();
      std::filesystem::rename(temp_path, final_path, error);
      if (error) {
        return false;
      }
      return FsyncDirectory(final_path.parent_path());
    }
    return false;
  }
  ::unlink(temp_path.c_str());
#endif
  return FsyncDirectory(final_path.parent_path());
}

bool PublishReplacingFile(const std::filesystem::path &temp_path, const std::filesystem::path &final_path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(temp_path, error) || error) {
    return false;
  }
  if (!SameDirectory(temp_path, final_path)) {
    return false;
  }
  if (!FsyncFile(temp_path)) {
    return false;
  }
#ifdef _WIN32
  if (!MoveFileExA(temp_path.string().c_str(), final_path.string().c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return false;
  }
#else
  std::filesystem::rename(temp_path, final_path, error);
  if (error) {
    return false;
  }
#endif
  return FsyncDirectory(final_path.parent_path());
}

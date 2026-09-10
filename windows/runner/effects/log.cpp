#include "log.h"

#include <windows.h>

#include <cstdio>
#include <fstream>
#include <mutex>

namespace effects {
namespace {

// Past this the file is truncated on the next open. A utility that runs all
// day must not grow an unbounded log.
const long long kMaxLogBytes = 512 * 1024;

std::mutex& LogMutex() {
  static std::mutex mutex;
  return mutex;
}

std::ofstream& LogStream() {
  static std::ofstream stream;
  return stream;
}

std::wstring ConfigDirectory() {
  wchar_t buffer[MAX_PATH] = {};
  const DWORD length =
      ::GetEnvironmentVariableW(L"APPDATA", buffer, static_cast<DWORD>(MAX_PATH));
  if (length == 0 || length >= static_cast<DWORD>(MAX_PATH)) {
    return std::wstring();
  }
  std::wstring path(buffer, length);
  path.append(L"\\WindowEffects");
  return path;
}

void WriteLine(const char* level, const std::string& message) {
  std::lock_guard<std::mutex> guard(LogMutex());
  std::ofstream& stream = LogStream();
  if (!stream.is_open()) {
    return;
  }

  SYSTEMTIME now = {};
  ::GetLocalTime(&now);

  char stamp[32] = {};
  ::_snprintf_s(stamp, sizeof(stamp), _TRUNCATE, "%02d:%02d:%02d.%03d",
                static_cast<int>(now.wHour), static_cast<int>(now.wMinute),
                static_cast<int>(now.wSecond),
                static_cast<int>(now.wMilliseconds));

  stream << stamp << "  " << level << "  " << message << '\n';
  stream.flush();
}

}  // namespace

void LogOpen() {
  std::lock_guard<std::mutex> guard(LogMutex());
  if (LogStream().is_open()) {
    return;
  }

  const std::wstring directory = ConfigDirectory();
  if (directory.empty()) {
    return;
  }
  ::CreateDirectoryW(directory.c_str(), nullptr);

  const std::wstring path = directory + L"\\engine.log";

  std::ios_base::openmode mode = std::ios::out | std::ios::app;
  WIN32_FILE_ATTRIBUTE_DATA attributes = {};
  if (::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
    const long long size =
        (static_cast<long long>(attributes.nFileSizeHigh) << 32) |
        static_cast<long long>(attributes.nFileSizeLow);
    if (size > kMaxLogBytes) {
      mode = std::ios::out | std::ios::trunc;
    }
  }

  LogStream().open(path.c_str(), mode);
}

void LogClose() {
  std::lock_guard<std::mutex> guard(LogMutex());
  if (LogStream().is_open()) {
    LogStream().close();
  }
}

void LogInfo(const std::string& message) {
  WriteLine("INFO ", message);
}

void LogWarn(const std::string& message) {
  WriteLine("WARN ", message);
}

void LogError(const std::string& message) {
  WriteLine("ERROR", message);
}

std::string Narrow(const std::wstring& value) {
  std::string result;
  result.reserve(value.size());
  for (size_t index = 0; index < value.size(); ++index) {
    const wchar_t character = value[index];
    result.push_back(character > 0 && character < 128
                         ? static_cast<char>(character)
                         : '?');
  }
  return result;
}

}  // namespace effects

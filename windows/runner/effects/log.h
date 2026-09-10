#ifndef RUNNER_EFFECTS_LOG_H_
#define RUNNER_EFFECTS_LOG_H_

#include <string>

namespace effects {

// Opens %APPDATA%\WindowEffects\engine.log, truncating it if it has grown past
// half a megabyte. Safe to call more than once; the second call does nothing.
void LogOpen();
void LogClose();

void LogInfo(const std::string& message);
void LogWarn(const std::string& message);
void LogError(const std::string& message);

// Narrows a wide string for logging. Anything outside ASCII becomes '?', which
// is fine for the executable and class names this log actually carries.
std::string Narrow(const std::wstring& value);

}  // namespace effects

#endif  // RUNNER_EFFECTS_LOG_H_

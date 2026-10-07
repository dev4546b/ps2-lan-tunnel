#pragma once

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace p2lt {
namespace util {

enum class LogLevel { Debug = 0, Info = 1, Warn = 2, Error = 3 };

void log_set_level(LogLevel level);
LogLevel log_get_level();
void log_write(LogLevel level, const char* fmt, ...);

#define P2LT_LOGI(...) ::p2lt::util::log_write(::p2lt::util::LogLevel::Info, __VA_ARGS__)
#define P2LT_LOGW(...) ::p2lt::util::log_write(::p2lt::util::LogLevel::Warn, __VA_ARGS__)
#define P2LT_LOGE(...) ::p2lt::util::log_write(::p2lt::util::LogLevel::Error, __VA_ARGS__)
#define P2LT_LOGD(...) ::p2lt::util::log_write(::p2lt::util::LogLevel::Debug, __VA_ARGS__)

}  // namespace util
}  // namespace p2lt
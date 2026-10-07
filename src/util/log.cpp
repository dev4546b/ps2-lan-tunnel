#include "util/log.h"

namespace p2lt {
namespace util {

static LogLevel g_log_level = LogLevel::Info;
static std::mutex g_log_mutex;

void log_set_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  g_log_level = level;
}

LogLevel log_get_level() { return g_log_level; }

void log_write(LogLevel level, const char* fmt, ...) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  if (static_cast<int>(level) < static_cast<int>(g_log_level)) return;

  time_t now = time(nullptr);
  tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &now);
#else
  localtime_r(&now, &tmv);
#endif
  char stamp[32];
  strftime(stamp, sizeof(stamp), "%H:%M:%S", &tmv);

  const char* tag = "DBG";
  switch (level) {
    case LogLevel::Info: tag = "INF"; break;
    case LogLevel::Warn: tag = "WRN"; break;
    case LogLevel::Error: tag = "ERR"; break;
    default: break;
  }

  fprintf(stderr, "[%s][%s] ", stamp, tag);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n");
  fflush(stderr);
}

}  // namespace util
}  // namespace p2lt
#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace hwdetect {

enum class LogLevel {
  Debug,
  Info,
  Warning,
  Error,
};

class Logger {
public:
  static Logger& instance() {
    static Logger logger;
    return logger;
  }

  void set_log_path(const std::string& path);
  void set_min_level(LogLevel level) { m_min_level = level; }
  void set_enabled(bool enabled) { m_enabled = enabled; }

  void debug(const std::string& msg)   { log(LogLevel::Debug,   msg); }
  void info(const std::string& msg)    { log(LogLevel::Info,    msg); }
  void warning(const std::string& msg) { log(LogLevel::Warning, msg); }
  void error(const std::string& msg)   { log(LogLevel::Error,   msg); }

private:
  Logger() = default;
  ~Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void log(LogLevel level, const std::string& msg);
  static const char* level_str(LogLevel level);

  std::string m_path;
  std::ofstream m_file;
  std::mutex m_mutex;
  LogLevel m_min_level = LogLevel::Info;
  bool m_enabled = true;
};

} // namespace hwdetect

#define HW_LOG_DEBUG(msg)   hwdetect::Logger::instance().debug(msg)
#define HW_LOG_INFO(msg)    hwdetect::Logger::instance().info(msg)
#define HW_LOG_WARNING(msg) hwdetect::Logger::instance().warning(msg)
#define HW_LOG_ERROR(msg)   hwdetect::Logger::instance().error(msg)

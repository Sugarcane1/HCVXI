#include "logger.h"
#include <chrono>
#include <ctime>
#include <filesystem>

namespace hwdetect {

Logger::~Logger() {
  if (m_file.is_open()) {
    m_file.close();
  }
}

void Logger::set_log_path(const std::string& path) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_file.is_open()) {
    m_file.close();
  }
  m_path = path;
  if (!m_path.empty()) {
    auto parent = std::filesystem::path(m_path).parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
      std::filesystem::create_directories(parent);
    }
    m_file.open(m_path, std::ios::out | std::ios::app);
  }
}

const char* Logger::level_str(LogLevel level) {
  switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO ";
    case LogLevel::Warning: return "WARN ";
    case LogLevel::Error:   return "ERROR";
  }
  return "???";
}

void Logger::log(LogLevel level, const std::string& msg) {
  if (!m_enabled || level < m_min_level) return;

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

  std::tm tm_buf;
#ifdef _WIN32
  localtime_s(&tm_buf, &time_t);
#else
  localtime_r(&time_t, &tm_buf);
#endif

  std::ostringstream line;
  line << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << " [" << level_str(level) << "] " << msg << "\n";

  std::string out = line.str();

  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_file.is_open()) {
    m_file << out;
    m_file.flush();
  }
#ifdef _DEBUG
  OutputDebugStringA(out.c_str());
#endif
}

} // namespace hwdetect

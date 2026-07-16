#include "file_logger.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>

namespace vynexos::core {

FileLogger::FileLogger(const std::string& file_path) {
    m_file.open(file_path, std::ios::app);
    if (!m_file.is_open()) {
        std::cerr << "CRITICAL: Failed to open log file: " << file_path << "\n";
    }
}

FileLogger::~FileLogger() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

void FileLogger::log_raw(LogLevel level, std::string_view message) {
    std::lock_guard lock(m_mutex);
    
    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    if (m_file.is_open()) {
        std::tm tm_struct{};
#ifdef _MSC_VER
        localtime_s(&tm_struct, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_struct);
#endif
        m_file << "[" << std::put_time(&tm_struct, "%F %T") << "] "
               << "[" << level_to_string(level) << "] "
               << message << std::endl;
    }
    
    // Also echo to console for VM testing
    std::cout << "[" << level_to_string(level) << "] " << message << std::endl;
}

std::string FileLogger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO ";
        case LogLevel::Warning: return "WARN ";
        case LogLevel::Error:   return "ERROR";
        case LogLevel::Fatal:   return "FATAL";
        default:                return "UNKNW";
    }
}

} // namespace vynexos::core

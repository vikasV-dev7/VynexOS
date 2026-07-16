#pragma once
#include "vynexos/core/logger.hpp"
#include <fstream>
#include <mutex>
#include <string>

namespace vynexos::core {

class FileLogger final : public ILogger {
public:
    explicit FileLogger(const std::string& file_path);
    ~FileLogger() override;

    void log_raw(LogLevel level, std::string_view message) override;

private:
    std::string level_to_string(LogLevel level) const;
    
    std::ofstream m_file;
    std::mutex m_mutex;
};

} // namespace vynexos::core

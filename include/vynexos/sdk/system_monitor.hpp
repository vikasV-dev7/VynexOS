#pragma once
#include <cstdint>

namespace vynexos::sdk {

struct SystemStats {
    double cpu_usage_percent;
    uint64_t ram_used_mb;
    uint64_t ram_total_mb;
    uint64_t vram_used_mb;
    uint64_t vram_total_mb;
    double ai_inference_load_percent;
};

class ISystemMonitor {
public:
    virtual ~ISystemMonitor() = default;
    
    // Polls system counters for real-time tracking
    virtual SystemStats get_current_stats() const = 0;
};

} // namespace vynexos::sdk

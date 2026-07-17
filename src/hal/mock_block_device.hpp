#pragma once
#include <vynexos/hal/storage.hpp>
#include <vynexos/core/logger.hpp>
#include <memory>
#include <vector>

namespace vynexos::hal {

class MockBlockDevice : public IBlockDevice {
public:
    // Initialize with a mock capacity in blocks. Block size is fixed at 512 bytes.
    explicit MockBlockDevice(std::shared_ptr<core::ILogger> logger, uint64_t capacity_blocks);
    ~MockBlockDevice() override;

    [[nodiscard]] std::expected<void, StorageError> initialize() override;
    
    [[nodiscard]] std::expected<void, StorageError> read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) override;
    [[nodiscard]] std::expected<void, StorageError> write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) override;
    
    [[nodiscard]] uint64_t get_capacity_blocks() const override;
    [[nodiscard]] size_t get_block_size() const override;
    
    void shutdown() override;

private:
    std::shared_ptr<core::ILogger> m_logger;
    uint64_t m_capacity_blocks;
    size_t m_block_size = 512;
    std::vector<uint8_t> m_storage;
    bool m_is_initialized = false;
};

} // namespace vynexos::hal

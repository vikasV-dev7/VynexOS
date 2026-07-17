#include "mock_block_device.hpp"
#include <cstring>

namespace vynexos::hal {

MockBlockDevice::MockBlockDevice(std::shared_ptr<core::ILogger> logger, uint64_t capacity_blocks)
    : m_logger(std::move(logger)), m_capacity_blocks(capacity_blocks) {}

MockBlockDevice::~MockBlockDevice() {
    shutdown();
}

std::expected<void, StorageError> MockBlockDevice::initialize() {
    if (m_is_initialized) return {};
    m_logger->info("MockBlockDevice: Initializing ramdisk with {} blocks ({} bytes/block).", m_capacity_blocks, m_block_size);
    m_storage.resize(m_capacity_blocks * m_block_size, 0);
    m_is_initialized = true;
    return {};
}

std::expected<void, StorageError> MockBlockDevice::read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) {
    if (!m_is_initialized) return std::unexpected(StorageError::DeviceNotReady);
    
    if (start_block + block_count > m_capacity_blocks) {
        m_logger->error("MockBlockDevice: Read out of bounds! start={}, count={}, capacity={}", start_block, block_count, m_capacity_blocks);
        return std::unexpected(StorageError::OutOfBounds);
    }
    
    size_t bytes_to_read = block_count * m_block_size;
    if (buffer.size() < bytes_to_read) {
        m_logger->error("MockBlockDevice: Read buffer too small!");
        return std::unexpected(StorageError::OutOfBounds);
    }
    
    m_logger->trace("MockBlockDevice: Reading {} blocks from {}", block_count, start_block);
    size_t byte_offset = start_block * m_block_size;
    std::memcpy(buffer.data(), m_storage.data() + byte_offset, bytes_to_read);
    
    return {};
}

std::expected<void, StorageError> MockBlockDevice::write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) {
    if (!m_is_initialized) return std::unexpected(StorageError::DeviceNotReady);
    
    size_t block_count = buffer.size() / m_block_size;
    if (buffer.size() % m_block_size != 0) {
        m_logger->error("MockBlockDevice: Write buffer size not a multiple of block size!");
        return std::unexpected(StorageError::OutOfBounds);
    }
    
    if (start_block + block_count > m_capacity_blocks) {
        m_logger->error("MockBlockDevice: Write out of bounds! start={}, count={}, capacity={}", start_block, block_count, m_capacity_blocks);
        return std::unexpected(StorageError::OutOfBounds);
    }
    
    m_logger->trace("MockBlockDevice: Writing {} blocks to {}", block_count, start_block);
    size_t byte_offset = start_block * m_block_size;
    std::memcpy(m_storage.data() + byte_offset, buffer.data(), buffer.size());
    
    return {};
}

uint64_t MockBlockDevice::get_capacity_blocks() const {
    return m_capacity_blocks;
}

size_t MockBlockDevice::get_block_size() const {
    return m_block_size;
}

void MockBlockDevice::shutdown() {
    if (!m_is_initialized) return;
    m_logger->info("MockBlockDevice: Shutting down.");
    m_storage.clear();
    m_is_initialized = false;
}

} // namespace vynexos::hal

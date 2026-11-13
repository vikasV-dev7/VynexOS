#include "mock_block_device.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace vynexos::hal {

MockBlockDevice::MockBlockDevice(std::shared_ptr<core::ILogger> logger, uint64_t capacity_blocks)
    : m_logger(std::move(logger)), m_capacity_blocks(capacity_blocks) {}

MockBlockDevice::~MockBlockDevice() {
    shutdown();
}

std::expected<void, StorageError> MockBlockDevice::initialize() {
    std::lock_guard lock(m_mutex);
    if (m_is_initialized) return {};
    // Every byte offset of the device must be representable, or no offset arithmetic below could be trusted.
    if (m_capacity_blocks > std::numeric_limits<uint64_t>::max() / m_block_size) {
        m_logger->error("MockBlockDevice: Capacity of {} blocks is not addressable.", m_capacity_blocks);
        return std::unexpected(StorageError::HardwareFailure);
    }
    // Nothing is allocated here: storage is created chunk by chunk as it is written.
    m_logger->info("MockBlockDevice: Initializing ramdisk with {} blocks ({} bytes/block), allocated on write.", m_capacity_blocks, m_block_size);
    m_is_initialized = true;
    return {};
}

bool MockBlockDevice::in_bounds(uint64_t start_block, uint64_t block_count) const noexcept {
    return block_count <= m_capacity_blocks && start_block <= m_capacity_blocks - block_count;
}

std::expected<void, StorageError> MockBlockDevice::read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) {
    std::lock_guard lock(m_mutex);
    if (!m_is_initialized) return std::unexpected(StorageError::DeviceNotReady);

    if (!in_bounds(start_block, block_count)) {
        m_logger->error("MockBlockDevice: Read out of bounds! start={}, count={}, capacity={}", start_block, block_count, m_capacity_blocks);
        return std::unexpected(StorageError::OutOfBounds);
    }

    // In bounds, so block_count <= capacity and the product cannot overflow (initialize() checked the capacity).
    const uint64_t bytes_to_read = uint64_t{block_count} * m_block_size;
    if (buffer.size() < bytes_to_read) {
        m_logger->error("MockBlockDevice: Read buffer too small!");
        return std::unexpected(StorageError::OutOfBounds);
    }

    m_logger->trace("MockBlockDevice: Reading {} blocks from {}", block_count, start_block);
    uint64_t offset = start_block * m_block_size;
    uint8_t* out = buffer.data();
    for (uint64_t remaining = bytes_to_read; remaining > 0;) {
        const uint64_t within = offset % kChunkBytes;
        const auto n = static_cast<std::size_t>(std::min<uint64_t>(remaining, kChunkBytes - within));
        if (const auto it = m_chunks.find(offset / kChunkBytes); it != m_chunks.end()) {
            std::memcpy(out, it->second.get() + within, n);
        } else {
            std::memset(out, 0, n); // never written
        }
        out += n;
        offset += n;
        remaining -= n;
    }

    return {};
}

std::expected<void, StorageError> MockBlockDevice::write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) {
    std::lock_guard lock(m_mutex);
    if (!m_is_initialized) return std::unexpected(StorageError::DeviceNotReady);

    size_t block_count = buffer.size() / m_block_size;
    if (buffer.size() % m_block_size != 0) {
        m_logger->error("MockBlockDevice: Write buffer size not a multiple of block size!");
        return std::unexpected(StorageError::OutOfBounds);
    }

    if (!in_bounds(start_block, block_count)) {
        m_logger->error("MockBlockDevice: Write out of bounds! start={}, count={}, capacity={}", start_block, block_count, m_capacity_blocks);
        return std::unexpected(StorageError::OutOfBounds);
    }

    m_logger->trace("MockBlockDevice: Writing {} blocks to {}", block_count, start_block);
    uint64_t offset = start_block * m_block_size;
    const uint8_t* in = buffer.data();
    for (uint64_t remaining = buffer.size(); remaining > 0;) {
        const uint64_t within = offset % kChunkBytes;
        const auto n = static_cast<std::size_t>(std::min<uint64_t>(remaining, kChunkBytes - within));
        const uint64_t index = offset / kChunkBytes;
        auto it = m_chunks.find(index);
        if (it == m_chunks.end() && std::any_of(in, in + n, [](uint8_t b) { return b != 0; })) {
            it = m_chunks.emplace(index, std::make_unique<uint8_t[]>(kChunkBytes)).first; // zero-filled
        }
        if (it != m_chunks.end()) std::memcpy(it->second.get() + within, in, n);
        // else: zeros into a region never written, which already reads as zeros
        in += n;
        offset += n;
        remaining -= n;
    }

    return {};
}

uint64_t MockBlockDevice::get_capacity_blocks() const {
    return m_capacity_blocks;
}

size_t MockBlockDevice::get_block_size() const {
    return m_block_size;
}

void MockBlockDevice::shutdown() {
    std::lock_guard lock(m_mutex);
    if (!m_is_initialized) return;
    m_logger->info("MockBlockDevice: Shutting down.");
    m_chunks.clear();
    m_is_initialized = false;
}

std::size_t MockBlockDevice::allocated_bytes() const {
    std::lock_guard lock(m_mutex);
    return m_chunks.size() * kChunkBytes;
}

} // namespace vynexos::hal

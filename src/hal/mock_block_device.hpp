#pragma once
#include <vynexos/hal/storage.hpp>
#include <vynexos/core/logger.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace vynexos::hal {

/// An in-memory block device (ramdisk) with a fixed logical capacity.
///
/// Storage is sparse: memory is allocated in fixed-size chunks the first time data is written into them, and blocks that
/// were never written read as zeros (as a freshly formatted disk would). Memory use therefore follows the data actually
/// stored, not the capacity: a 512 MiB device costs nothing until it is written to. Writing zeros into a region that was
/// never allocated allocates nothing. Thread-safe.
class MockBlockDevice : public IBlockDevice {
public:
    /// The allocation unit: 128 blocks of 512 bytes.
    static constexpr std::size_t kChunkBytes = 64 * 1024;

    // Initialize with a mock capacity in blocks. Block size is fixed at 512 bytes.
    explicit MockBlockDevice(std::shared_ptr<core::ILogger> logger, uint64_t capacity_blocks);
    ~MockBlockDevice() override;

    [[nodiscard]] std::expected<void, StorageError> initialize() override;

    [[nodiscard]] std::expected<void, StorageError> read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) override;
    [[nodiscard]] std::expected<void, StorageError> write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) override;

    [[nodiscard]] uint64_t get_capacity_blocks() const override;
    [[nodiscard]] size_t get_block_size() const override;

    void shutdown() override;

    /// Bytes of storage currently allocated (a whole number of chunks). For diagnostics and tests.
    [[nodiscard]] std::size_t allocated_bytes() const;

private:
    /// True when blocks [start_block, start_block + block_count) lie within the device. Written so that no
    /// intermediate value can wrap around, whatever the arguments.
    [[nodiscard]] bool in_bounds(uint64_t start_block, uint64_t block_count) const noexcept;

    std::shared_ptr<core::ILogger> m_logger;
    uint64_t m_capacity_blocks;
    size_t m_block_size = 512;
    mutable std::mutex m_mutex;
    std::unordered_map<uint64_t, std::unique_ptr<uint8_t[]>> m_chunks; // chunk index -> kChunkBytes bytes
    bool m_is_initialized = false;
};

} // namespace vynexos::hal

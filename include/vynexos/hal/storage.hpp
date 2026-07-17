#pragma once
#include <expected>
#include <cstdint>
#include <span>

namespace vynexos::hal {

enum class StorageError {
    DeviceNotReady,
    OutOfBounds,
    HardwareFailure,
    ReadOnly
};

class IBlockDevice {
public:
    virtual ~IBlockDevice() = default;
    
    [[nodiscard]] virtual std::expected<void, StorageError> initialize() = 0;
    
    // Reads a sequence of blocks synchronously.
    [[nodiscard]] virtual std::expected<void, StorageError> read_blocks(uint64_t start_block, size_t block_count, std::span<uint8_t> buffer) = 0;
    
    // Writes a sequence of blocks synchronously.
    [[nodiscard]] virtual std::expected<void, StorageError> write_blocks(uint64_t start_block, std::span<const uint8_t> buffer) = 0;
    
    // Returns the total capacity in blocks.
    [[nodiscard]] virtual uint64_t get_capacity_blocks() const = 0;
    
    // Returns the size of a single block in bytes.
    [[nodiscard]] virtual size_t get_block_size() const = 0;
    
    virtual void shutdown() = 0;
};

} // namespace vynexos::hal

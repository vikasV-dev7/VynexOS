#pragma once
#include <cstdint>
#include <span>

namespace vynexos::core {

/**
 * @brief Represents an open, stateful handle to a file.
 * 
 * Responsibility: Byte-oriented read/write operations and cursor positioning.
 * Ownership: Owned dynamically (e.g., via std::unique_ptr) by the consumer.
 * Lifetime: Destructor automatically closes resources (RAII).
 * Thread Safety: Single-threaded by design. Requires external synchronization for concurrent access.
 */
class IFileHandle {
public:
    virtual ~IFileHandle() = default;

    /**
     * @brief Reads up to buffer.size() bytes from the current cursor position.
     * @param buffer Destination buffer.
     * @return Number of bytes actually read.
     */
    [[nodiscard]] virtual std::size_t read(std::span<std::uint8_t> buffer) = 0;

    /**
     * @brief Writes buffer.size() bytes at the current cursor position.
     * @param buffer Source buffer.
     * @return Number of bytes actually written.
     */
    virtual std::size_t write(std::span<const std::uint8_t> buffer) = 0;

    /**
     * @brief Moves the read/write cursor to the specified absolute offset.
     * @param offset The byte offset from the beginning of the file.
     * @return true if successful, false if out of bounds.
     */
    virtual bool seek(std::size_t offset) noexcept = 0;

    /**
     * @brief Returns the current cursor position.
     */
    [[nodiscard]] virtual std::size_t position() const noexcept = 0;

    /**
     * @brief Returns the total size of the file in bytes.
     */
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

    /**
     * @brief Flushes any buffered writes to the underlying node.
     */
    virtual void flush() = 0;
};

} // namespace vynexos::core

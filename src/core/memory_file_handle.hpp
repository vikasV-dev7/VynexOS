#pragma once
#include "vynexos/core/file_handle.hpp"
#include "vnode.hpp"
#include <memory>

namespace vynexos::core::internal {

/**
 * @brief Concrete implementation of IFileHandle for the MemoryFileSystem.
 */
class MemoryFileHandle final : public IFileHandle {
public:
    explicit MemoryFileHandle(std::shared_ptr<FileNode> file_node);
    ~MemoryFileHandle() override = default;

    // Delete copy, allow move
    MemoryFileHandle(const MemoryFileHandle&) = delete;
    MemoryFileHandle& operator=(const MemoryFileHandle&) = delete;
    MemoryFileHandle(MemoryFileHandle&&) noexcept = default;
    MemoryFileHandle& operator=(MemoryFileHandle&&) noexcept = default;

    [[nodiscard]] std::size_t read(std::span<std::uint8_t> buffer) override;
    std::size_t write(std::span<const std::uint8_t> buffer) override;
    bool seek(std::size_t offset) noexcept override;
    [[nodiscard]] std::size_t position() const noexcept override;
    [[nodiscard]] std::size_t size() const noexcept override;
    void flush() override;

private:
    std::shared_ptr<FileNode> m_node;
    std::size_t m_cursor{0};
};

} // namespace vynexos::core::internal

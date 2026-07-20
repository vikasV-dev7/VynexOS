#include "memory_file_handle.hpp"
#include <algorithm>
#include <cstring>

namespace vynexos::core::internal {

MemoryFileHandle::MemoryFileHandle(std::shared_ptr<FileNode> file_node)
    : m_node(std::move(file_node)), m_cursor(0) {
}

std::size_t MemoryFileHandle::read(std::span<std::uint8_t> buffer) {
    if (buffer.empty() || !m_node) return 0;

    // Thread safety: Read concurrency
    // Use shared_lock to allow concurrent readers on the same FileNode
    std::shared_lock lock(m_node->mutex);

    if (m_cursor >= m_node->data.size()) {
        return 0; // EOF
    }

    std::size_t available = m_node->data.size() - m_cursor;
    std::size_t to_read = std::min(available, buffer.size());

    std::memcpy(buffer.data(), m_node->data.data() + m_cursor, to_read);
    
    // The handle itself is not thread safe (IFileHandle contract), 
    // so we can update m_cursor safely without an internal mutex.
    m_cursor += to_read;

    return to_read;
}

std::size_t MemoryFileHandle::write(std::span<const std::uint8_t> buffer) {
    if (buffer.empty() || !m_node) return 0;

    // Thread safety: Write exclusivity
    // Use unique_lock to prevent concurrent writes/reads on the same FileNode
    std::unique_lock lock(m_node->mutex);

    std::size_t required_capacity = m_cursor + buffer.size();
    if (required_capacity > m_node->data.size()) {
        m_node->data.resize(required_capacity);
    }

    std::memcpy(m_node->data.data() + m_cursor, buffer.data(), buffer.size());
    m_cursor += buffer.size();

    return buffer.size();
}

bool MemoryFileHandle::seek(std::size_t offset) noexcept {
    if (!m_node) return false;

    std::shared_lock lock(m_node->mutex);
    if (offset > m_node->data.size()) {
        return false; // Out of bounds
    }
    m_cursor = offset;
    return true;
}

std::size_t MemoryFileHandle::position() const noexcept {
    return m_cursor;
}

std::size_t MemoryFileHandle::size() const noexcept {
    if (!m_node) return 0;
    std::shared_lock lock(m_node->mutex);
    return m_node->data.size();
}

void MemoryFileHandle::flush() {
    // No-op for purely memory-based file handle
}

} // namespace vynexos::core::internal

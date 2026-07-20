#pragma once
#include "vynexos/core/virtual_file_system.hpp"
#include <shared_mutex>
#include <memory>

// Forward declarations for internal object model
namespace vynexos::core::internal {
    class DirectoryNode;
    class VNode;
}

namespace vynexos::core {

/**
 * @brief Concrete implementation of IVirtualFileSystem representing an in-memory VFS.
 * 
 * Responsibility: Manages the node hierarchy, path resolution, and creates file handles.
 * Thread Safety: Thread-safe. Uses a global std::shared_mutex to protect tree structure.
 */
class MemoryFileSystem final : public IVirtualFileSystem {
public:
    MemoryFileSystem();
    ~MemoryFileSystem() override = default;

    // Delete copy/move for safety
    MemoryFileSystem(const MemoryFileSystem&) = delete;
    MemoryFileSystem& operator=(const MemoryFileSystem&) = delete;
    MemoryFileSystem(MemoryFileSystem&&) = delete;
    MemoryFileSystem& operator=(MemoryFileSystem&&) = delete;

    [[nodiscard]] std::unique_ptr<IFileHandle> open(const std::string& absolute_path) override;
    [[nodiscard]] std::unique_ptr<IFileHandle> create(const std::string& absolute_path) override;
    bool remove(const std::string& absolute_path) override;
    [[nodiscard]] bool exists(const std::string& absolute_path) const noexcept override;
    [[nodiscard]] std::optional<FileStat> stat(const std::string& absolute_path) const override;
    bool create_directory(const std::string& absolute_path) override;
    [[nodiscard]] std::vector<std::string> list_directory(const std::string& absolute_path) const override;

private:
    std::shared_ptr<internal::DirectoryNode> m_root;
    
    // Global tree lock: Protects insertions, deletions, and structural lookups within the hierarchy.
    // Must be acquired BEFORE any VNode::mutex to prevent deadlocks.
    mutable std::shared_mutex m_tree_mutex;

    // Internal helpers (must be called with m_tree_mutex held appropriately)
    std::shared_ptr<internal::VNode> resolve_node_internal(const std::string& absolute_path) const;
    bool resolve_parent_internal(const std::string& absolute_path, std::shared_ptr<internal::DirectoryNode>& out_parent, std::string& out_filename) const;
};

} // namespace vynexos::core

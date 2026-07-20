#include "vynexos/core/memory_file_system.hpp"
#include "vynexos/core/virtual_path.hpp"
#include "vnode.hpp"
#include "memory_file_handle.hpp"

namespace vynexos::core {

MemoryFileSystem::MemoryFileSystem() 
    : m_root(std::make_shared<internal::DirectoryNode>("/")) {
}

std::shared_ptr<internal::VNode> MemoryFileSystem::resolve_node_internal(const std::string& absolute_path) const {
    VirtualPath path(absolute_path);
    if (!path.is_absolute()) return nullptr;
    if (path.is_root()) return m_root;

    std::shared_ptr<internal::VNode> current = m_root;
    
    for (const auto& segment : path.segments()) {
        if (current->get_type() != internal::VNodeType::Directory) {
            return nullptr; // Path traverses through a file
        }
        
        auto dir = std::static_pointer_cast<internal::DirectoryNode>(current);
        auto next = dir->get_child(segment);
        if (!next) {
            return nullptr;
        }
        current = next;
    }
    
    return current;
}

bool MemoryFileSystem::resolve_parent_internal(const std::string& absolute_path, std::shared_ptr<internal::DirectoryNode>& out_parent, std::string& out_filename) const {
    VirtualPath path(absolute_path);
    if (!path.is_absolute() || path.is_root()) return false;

    VirtualPath parent_path = path.parent();
    auto parent_node = resolve_node_internal(parent_path.as_string());
    
    if (!parent_node || parent_node->get_type() != internal::VNodeType::Directory) {
        return false;
    }

    out_parent = std::static_pointer_cast<internal::DirectoryNode>(parent_node);
    out_filename = std::string(path.filename());
    return true;
}

std::unique_ptr<IFileHandle> MemoryFileSystem::open(const std::string& absolute_path) {
    std::shared_lock lock(m_tree_mutex);
    auto node = resolve_node_internal(absolute_path);
    if (!node || node->get_type() != internal::VNodeType::File) {
        return nullptr;
    }
    return std::make_unique<internal::MemoryFileHandle>(std::static_pointer_cast<internal::FileNode>(node));
}

std::unique_ptr<IFileHandle> MemoryFileSystem::create(const std::string& absolute_path) {
    std::unique_lock lock(m_tree_mutex);
    
    // Check if it exists
    auto node = resolve_node_internal(absolute_path);
    if (node) {
        if (node->get_type() != internal::VNodeType::File) {
            return nullptr; // Exists but is a directory
        }
        // Truncate existing file
        auto file_node = std::static_pointer_cast<internal::FileNode>(node);
        std::unique_lock file_lock(file_node->mutex);
        file_node->data.clear();
        return std::make_unique<internal::MemoryFileHandle>(file_node);
    }
    
    // Try to create it
    std::shared_ptr<internal::DirectoryNode> parent;
    std::string filename;
    if (!resolve_parent_internal(absolute_path, parent, filename)) {
        return nullptr;
    }
    
    auto new_file = std::make_shared<internal::FileNode>(filename);
    parent->add_child(new_file);
    
    return std::make_unique<internal::MemoryFileHandle>(new_file);
}

bool MemoryFileSystem::remove(const std::string& absolute_path) {
    std::unique_lock lock(m_tree_mutex);
    
    std::shared_ptr<internal::DirectoryNode> parent;
    std::string filename;
    if (!resolve_parent_internal(absolute_path, parent, filename)) {
        return false;
    }
    
    auto child = parent->get_child(filename);
    if (!child) return false;
    
    // If it's a directory, it must be empty to be removed
    if (child->get_type() == internal::VNodeType::Directory) {
        auto dir = std::static_pointer_cast<internal::DirectoryNode>(child);
        if (!dir->children.empty()) {
            return false;
        }
    }
    
    parent->remove_child(filename);
    return true;
}

bool MemoryFileSystem::exists(const std::string& absolute_path) const noexcept {
    std::shared_lock lock(m_tree_mutex);
    return resolve_node_internal(absolute_path) != nullptr;
}

std::optional<FileStat> MemoryFileSystem::stat(const std::string& absolute_path) const {
    std::shared_lock lock(m_tree_mutex);
    auto node = resolve_node_internal(absolute_path);
    if (!node) return std::nullopt;
    
    FileStat result;
    result.is_directory = (node->get_type() == internal::VNodeType::Directory);
    if (!result.is_directory) {
        auto file = std::static_pointer_cast<internal::FileNode>(node);
        std::shared_lock file_lock(file->mutex);
        result.size_bytes = file->get_size();
    } else {
        result.size_bytes = 0;
    }
    return result;
}

bool MemoryFileSystem::create_directory(const std::string& absolute_path) {
    std::unique_lock lock(m_tree_mutex);
    
    if (resolve_node_internal(absolute_path)) {
        return false; // Already exists
    }
    
    std::shared_ptr<internal::DirectoryNode> parent;
    std::string dirname;
    if (!resolve_parent_internal(absolute_path, parent, dirname)) {
        return false;
    }
    
    auto new_dir = std::make_shared<internal::DirectoryNode>(dirname);
    parent->add_child(new_dir);
    
    return true;
}

std::vector<std::string> MemoryFileSystem::list_directory(const std::string& absolute_path) const {
    std::vector<std::string> result;
    std::shared_lock lock(m_tree_mutex);
    
    auto node = resolve_node_internal(absolute_path);
    if (node && node->get_type() == internal::VNodeType::Directory) {
        auto dir = std::static_pointer_cast<internal::DirectoryNode>(node);
        for (const auto& [name, child] : dir->children) {
            result.push_back(name);
        }
    }
    
    return result;
}

} // namespace vynexos::core

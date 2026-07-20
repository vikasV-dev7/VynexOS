#pragma once
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <shared_mutex>
#include <cstdint>

namespace vynexos::core::internal {

enum class VNodeType {
    File,
    Directory
};

class DirectoryNode; // Forward declaration

/**
 * @brief Base class for all virtual filesystem nodes.
 * 
 * Internal structure. Not exposed to public API.
 */
class VNode {
public:
    VNode(std::string name, VNodeType type)
        : m_name(std::move(name)), m_type(type) {}

    virtual ~VNode() = default;

    [[nodiscard]] const std::string& get_name() const noexcept { return m_name; }
    [[nodiscard]] VNodeType get_type() const noexcept { return m_type; }

    void set_parent(std::weak_ptr<DirectoryNode> parent) {
        m_parent = std::move(parent);
    }
    
    [[nodiscard]] std::weak_ptr<DirectoryNode> get_parent() const noexcept {
        return m_parent;
    }

    // A node-level lock to protect its own contents/metadata.
    // Lock ordering: If acquiring both a tree lock and a node lock, the tree lock MUST be acquired first.
    mutable std::shared_mutex mutex;

protected:
    std::string m_name;
    VNodeType m_type;
    std::weak_ptr<DirectoryNode> m_parent;
};

/**
 * @brief A node representing a file and its binary contents.
 */
class FileNode final : public VNode {
public:
    explicit FileNode(std::string name)
        : VNode(std::move(name), VNodeType::File) {}

    // Data buffer, protected externally via the VNode::mutex
    std::vector<uint8_t> data;

    // Helper to get size safely (assuming lock is held)
    [[nodiscard]] std::size_t get_size() const noexcept {
        return data.size();
    }
};

/**
 * @brief A node representing a directory and its children.
 */
class DirectoryNode final : public VNode, public std::enable_shared_from_this<DirectoryNode> {
public:
    explicit DirectoryNode(std::string name)
        : VNode(std::move(name), VNodeType::Directory) {}

    // Children map, protected externally via the VNode::mutex
    std::map<std::string, std::shared_ptr<VNode>> children;

    /**
     * @brief Helper to add a child and correctly establish the weak_ptr parent link.
     */
    void add_child(std::shared_ptr<VNode> child) {
        child->set_parent(weak_from_this());
        children[child->get_name()] = std::move(child);
    }

    /**
     * @brief Helper to remove a child by name.
     */
    void remove_child(const std::string& child_name) {
        children.erase(child_name);
    }

    /**
     * @brief Helper to find a child by name.
     */
    [[nodiscard]] std::shared_ptr<VNode> get_child(const std::string& child_name) const {
        auto it = children.find(child_name);
        if (it != children.end()) {
            return it->second;
        }
        return nullptr;
    }
};

} // namespace vynexos::core::internal

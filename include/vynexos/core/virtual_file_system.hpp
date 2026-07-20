#pragma once
#include "vynexos/core/file_handle.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace vynexos::core {

/**
 * @brief Metadata for a file or directory in the Virtual File System.
 */
struct FileStat {
    std::size_t size_bytes{0};
    bool is_directory{false};
};

/**
 * @brief The Virtual File System (VFS) boundary interface.
 * 
 * Responsibility: Namespace resolution, directory enumeration, and file creation/deletion.
 * Ownership: Owned by the CompositionRoot. Injected via interface.
 * Lifetime: Scope of the operating system runtime.
 * Thread Safety: Thread-compatible. Implementations must synchronize path traversal internally.
 */
class IVirtualFileSystem {
public:
    virtual ~IVirtualFileSystem() = default;

    /**
     * @brief Opens an existing file for data operations.
     * @param absolute_path The fully qualified path.
     * @return Unique pointer to an IFileHandle, or nullptr if the file does not exist.
     */
    [[nodiscard]] virtual std::unique_ptr<IFileHandle> open(const std::string& absolute_path) = 0;

    /**
     * @brief Creates a new file or truncates an existing one.
     * @param absolute_path The fully qualified path.
     * @return Unique pointer to the newly created IFileHandle, or nullptr on failure.
     */
    [[nodiscard]] virtual std::unique_ptr<IFileHandle> create(const std::string& absolute_path) = 0;

    /**
     * @brief Deletes a file or empty directory.
     * @param absolute_path The fully qualified path.
     * @return true if successfully removed.
     */
    virtual bool remove(const std::string& absolute_path) = 0;

    /**
     * @brief Checks if a path exists.
     * @param absolute_path The fully qualified path.
     * @return true if the node exists.
     */
    [[nodiscard]] virtual bool exists(const std::string& absolute_path) const noexcept = 0;

    /**
     * @brief Retrieves metadata for a file or directory.
     * @param absolute_path The fully qualified path.
     * @return A FileStat object if the path exists, std::nullopt otherwise.
     */
    [[nodiscard]] virtual std::optional<FileStat> stat(const std::string& absolute_path) const = 0;

    /**
     * @brief Creates a new directory.
     * @param absolute_path The fully qualified path.
     * @return true if created successfully.
     */
    virtual bool create_directory(const std::string& absolute_path) = 0;

    /**
     * @brief Enumerates the children of a directory.
     * @param absolute_path The directory to list.
     * @return A vector of child names. Empty if not a directory or does not exist.
     */
    [[nodiscard]] virtual std::vector<std::string> list_directory(const std::string& absolute_path) const = 0;
};

} // namespace vynexos::core

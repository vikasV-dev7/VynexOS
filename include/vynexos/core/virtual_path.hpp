#pragma once
#include <string>
#include <vector>
#include <string_view>
#include <compare>

namespace vynexos::core {

/**
 * @brief Represents an immutable, normalized, platform-independent filesystem path.
 * 
 * Responsibility: Normalize path separators, resolve relative tokens ("." and ".."),
 * provide equality comparison, and expose path components for VFS traversal.
 * Ownership: Immutable value object, can be copied or moved.
 * Thread Safety: Thread-safe (immutable).
 */
class VirtualPath {
public:
    /**
     * @brief Constructs and normalizes a VirtualPath from a string.
     * @param raw_path The input path string (e.g., "/usr/bin/.././etc").
     */
    explicit VirtualPath(std::string_view raw_path);

    /**
     * @brief Default constructor creating an empty path.
     */
    VirtualPath() = default;

    /**
     * @brief Returns the fully normalized path string.
     */
    [[nodiscard]] const std::string& as_string() const noexcept;

    /**
     * @brief Returns true if the path is absolute (starts with '/').
     */
    [[nodiscard]] bool is_absolute() const noexcept;

    /**
     * @brief Returns true if the path points to the root directory ("/").
     */
    [[nodiscard]] bool is_root() const noexcept;

    /**
     * @brief Returns true if the path is completely empty.
     */
    [[nodiscard]] bool is_empty() const noexcept;

    /**
     * @brief Returns the parent path, or the same path if it is already root.
     */
    [[nodiscard]] VirtualPath parent() const;

    /**
     * @brief Returns just the filename component (the last segment).
     */
    [[nodiscard]] std::string_view filename() const noexcept;

    /**
     * @brief Returns the file extension (including the dot), or empty if none.
     */
    [[nodiscard]] std::string_view extension() const noexcept;

    /**
     * @brief Returns a reference to the individual normalized path segments.
     */
    [[nodiscard]] const std::vector<std::string>& segments() const noexcept;

    /**
     * @brief Joins this path with another path segment.
     */
    [[nodiscard]] VirtualPath append(std::string_view component) const;

    // Standard comparisons
    auto operator<=>(const VirtualPath&) const = default;

private:
    std::string m_normalized_string;
    std::vector<std::string> m_segments;
    bool m_is_absolute{false};

    void normalize(std::string_view raw_path);
};

} // namespace vynexos::core

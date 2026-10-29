#pragma once
#include "vynexos/desktop/desktop_types.hpp"
#include <vector>
#include <optional>
namespace vynexos::desktop {

// Encapsulates dirty-region management, designed to support future spatial partitioning
class DirtyRegionSet {
public:
    DirtyRegionSet() = default;
    ~DirtyRegionSet() = default;

    void add_region(const Rect& rect);
    void clear();
    bool is_empty() const;

    // Get the merged non-overlapping rectangles
    std::vector<Rect> get_regions() const;

    // Diagnostic information
    size_t get_region_count() const;
    size_t get_total_area() const;

private:
    std::vector<Rect> m_regions;

    // Helper functions
    bool rects_overlap(const Rect& a, const Rect& b) const;
    Rect merge_rects(const Rect& a, const Rect& b) const;
    void merge_internal();
};

// Utility functions for Rect intersection
std::optional<Rect> intersect_rects(const Rect& a, const Rect& b);

} // namespace vynexos::desktop

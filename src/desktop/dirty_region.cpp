#include "vynexos/desktop/dirty_region.hpp"
#include <algorithm>
#include <cmath>

namespace vynexos::desktop {

void DirtyRegionSet::add_region(const Rect& rect) {
    if (rect.width == 0 || rect.height == 0) return;
    m_regions.push_back(rect);
    merge_internal();
}

void DirtyRegionSet::clear() {
    m_regions.clear();
}

bool DirtyRegionSet::is_empty() const {
    return m_regions.empty();
}

std::vector<Rect> DirtyRegionSet::get_regions() const {
    return m_regions;
}

size_t DirtyRegionSet::get_region_count() const {
    return m_regions.size();
}

size_t DirtyRegionSet::get_total_area() const {
    size_t area = 0;
    for (const auto& r : m_regions) {
        area += static_cast<size_t>(r.width) * static_cast<size_t>(r.height);
    }
    return area;
}

bool DirtyRegionSet::rects_overlap(const Rect& a, const Rect& b) const {
    return !(a.x >= b.x + static_cast<int32_t>(b.width) ||
             a.x + static_cast<int32_t>(a.width) <= b.x ||
             a.y >= b.y + static_cast<int32_t>(b.height) ||
             a.y + static_cast<int32_t>(a.height) <= b.y);
}

Rect DirtyRegionSet::merge_rects(const Rect& a, const Rect& b) const {
    int32_t min_x = std::min(a.x, b.x);
    int32_t min_y = std::min(a.y, b.y);
    int32_t max_x = std::max(a.x + static_cast<int32_t>(a.width), b.x + static_cast<int32_t>(b.width));
    int32_t max_y = std::max(a.y + static_cast<int32_t>(a.height), b.y + static_cast<int32_t>(b.height));
    return {min_x, min_y, static_cast<uint32_t>(max_x - min_x), static_cast<uint32_t>(max_y - min_y)};
}

void DirtyRegionSet::merge_internal() {
    bool merged_any = true;
    while (merged_any) {
        merged_any = false;
        for (size_t i = 0; i < m_regions.size(); ++i) {
            for (size_t j = i + 1; j < m_regions.size(); ++j) {
                if (rects_overlap(m_regions[i], m_regions[j])) {
                    m_regions[i] = merge_rects(m_regions[i], m_regions[j]);
                    m_regions.erase(m_regions.begin() + static_cast<std::ptrdiff_t>(j));
                    merged_any = true;
                    break;
                }
            }
            if (merged_any) break;
        }
    }
}

std::optional<Rect> intersect_rects(const Rect& a, const Rect& b) {
    int32_t min_x = std::max(a.x, b.x);
    int32_t min_y = std::max(a.y, b.y);
    int32_t max_x = std::min(a.x + static_cast<int32_t>(a.width), b.x + static_cast<int32_t>(b.width));
    int32_t max_y = std::min(a.y + static_cast<int32_t>(a.height), b.y + static_cast<int32_t>(b.height));
    
    if (min_x < max_x && min_y < max_y) {
        return Rect{min_x, min_y, static_cast<uint32_t>(max_x - min_x), static_cast<uint32_t>(max_y - min_y)};
    }
    return std::nullopt;
}

} // namespace vynexos::desktop

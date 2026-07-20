#include "vynexos/core/virtual_path.hpp"
#include <sstream>

namespace vynexos::core {

VirtualPath::VirtualPath(std::string_view raw_path) {
    normalize(raw_path);
}

void VirtualPath::normalize(std::string_view raw_path) {
    if (raw_path.empty()) {
        m_normalized_string = "";
        m_is_absolute = false;
        return;
    }

    m_is_absolute = (raw_path.front() == '/');

    std::vector<std::string> temp_segments;
    std::string current_segment;

    auto push_segment = [&]() {
        if (current_segment == "..") {
            if (!temp_segments.empty() && temp_segments.back() != "..") {
                temp_segments.pop_back();
            } else if (!m_is_absolute || (m_is_absolute && !temp_segments.empty())) {
                // If relative, keep ".." 
                // If absolute, going above root stays at root, so we only push if we have some non-root path.
                // Wait, if it is absolute, and we are at root, ".." is just ignored.
                // If it is absolute, and we are at some path, the pop_back would have hit if the last was not "..".
                // If it is absolute and temp_segments is empty, we do nothing.
                if (!m_is_absolute) {
                    temp_segments.push_back("..");
                }
            }
        } else if (!current_segment.empty() && current_segment != ".") {
            temp_segments.push_back(std::move(current_segment));
        }
        current_segment.clear();
    };

    for (char c : raw_path) {
        if (c == '/' || c == '\\') {
            push_segment();
        } else {
            current_segment += c;
        }
    }
    push_segment();

    m_segments = std::move(temp_segments);

    // Rebuild normalized string
    if (m_is_absolute) {
        m_normalized_string = "/";
    }
    
    for (size_t i = 0; i < m_segments.size(); ++i) {
        m_normalized_string += m_segments[i];
        if (i < m_segments.size() - 1) {
            m_normalized_string += "/";
        }
    }
}

const std::string& VirtualPath::as_string() const noexcept {
    return m_normalized_string;
}

bool VirtualPath::is_absolute() const noexcept {
    return m_is_absolute;
}

bool VirtualPath::is_root() const noexcept {
    return m_is_absolute && m_segments.empty();
}

bool VirtualPath::is_empty() const noexcept {
    return m_normalized_string.empty();
}

VirtualPath VirtualPath::parent() const {
    if (is_root() || is_empty()) {
        return *this;
    }
    if (!m_is_absolute && !m_segments.empty() && m_segments.back() == "..") {
        VirtualPath p = *this;
        p.m_segments.push_back("..");
        p.m_normalized_string += "/..";
        return p;
    }

    if (m_segments.empty()) return *this;

    VirtualPath p;
    p.m_is_absolute = m_is_absolute;
    p.m_segments.assign(m_segments.begin(), m_segments.end() - 1);
    
    if (p.m_is_absolute) {
        p.m_normalized_string = "/";
    }
    for (size_t i = 0; i < p.m_segments.size(); ++i) {
        p.m_normalized_string += p.m_segments[i];
        if (i < p.m_segments.size() - 1) {
            p.m_normalized_string += "/";
        }
    }
    return p;
}

std::string_view VirtualPath::filename() const noexcept {
    if (m_segments.empty()) return "";
    return m_segments.back();
}

std::string_view VirtualPath::extension() const noexcept {
    std::string_view name = filename();
    if (name.empty() || name == "." || name == "..") return "";
    
    auto dot_pos = name.find_last_of('.');
    if (dot_pos == std::string_view::npos || dot_pos == 0) {
        return "";
    }
    return name.substr(dot_pos);
}

const std::vector<std::string>& VirtualPath::segments() const noexcept {
    return m_segments;
}

VirtualPath VirtualPath::append(std::string_view component) const {
    if (component.empty()) return *this;
    
    std::string joined = m_normalized_string;
    if (!joined.empty() && joined.back() != '/') {
        joined += '/';
    }
    joined += component;
    
    return VirtualPath(joined);
}

} // namespace vynexos::core

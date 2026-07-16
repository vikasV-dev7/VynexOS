#pragma once
#include <string_view>
#include <vector>
#include <string>
#include <functional>

namespace vynexos::apps {

struct FileNode {
    std::string name;
    std::string path;
    bool is_directory;
    uint64_t size_bytes;
};

class IFileExplorer {
public:
    virtual ~IFileExplorer() = default;
    
    // Traditional hierarchical navigation
    virtual std::vector<FileNode> list_directory(std::string_view path) = 0;
    
    // AI Semantic Search (e.g. "Find documents about the Q3 budget")
    virtual void semantic_search_async(std::string_view query, std::function<void(std::vector<FileNode>)> callback) = 0;
};

} // namespace vynexos::apps

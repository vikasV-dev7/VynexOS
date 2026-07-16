<!-- AUTO-GENERATED START -->
# [[IFileExplorer]]

## Purpose
Interface for [[IFileExplorer]].

## Namespace
`vynexos::apps`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/apps/file_explorer.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IFileExplorer]]() = default`
- `virtual std::vector<FileNode> list_directory(std::string_view path) = 0`
- `virtual void semantic_search_async(std::string_view query, std::function<void(std::vector<FileNode>)> callback) = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->

<!-- AUTO-GENERATED START -->
# [[BasicFileExplorer]]

## Purpose & Responsibilities
_Not explicitly documented._

## Dependencies & Interaction with CompositionRoot
_Constructor injected dependencies (Refer to Source Code)._

## Failure recovery & Performance considerations
**Failure Recovery**: _Unspecified_
**Performance**: _Unspecified_

## Extension points
_No explicit extension points defined._

## Public API
- `std::vector<FileNode> list_directory(std::string_view path) override;`
- `void semantic_search_async(std::string_view query, std::function<void(std::vector<FileNode>)> callback) override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:39.977595+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/apps/basic_file_explorer.hpp`

<!-- AUTO-GENERATED END -->



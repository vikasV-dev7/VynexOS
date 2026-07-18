<!-- AUTO-GENERATED START -->
# [[IFileExplorer]]

## Why the interface exists & Architectural role
_Not explicitly documented._

## Lifetime & Ownership expectations
**Lifetime**: _Unspecified_
**Ownership**: _Unspecified_

## Threading guarantees & Failure behaviour
**Thread Safety**: _Unspecified_
**Failure Handling**: _Unspecified_

## Extension guidance
_No explicit extension points defined._

## Public API
- `virtual ~[[IFileExplorer]]() = default;`
- `virtual std::vector<FileNode> list_directory(std::string_view path) = 0;`
- `virtual void semantic_search_async(std::string_view query, std::function<void(std::vector<FileNode>)> callback) = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:39.554495+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/apps/file_explorer.hpp`

<!-- AUTO-GENERATED END -->



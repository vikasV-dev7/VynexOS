<!-- AUTO-GENERATED START -->
# [[IPluginManager]]

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
- `virtual ~[[IPluginManager]]() = default;`
- `virtual std::expected<std::shared_ptr<[[IPlugin]]>, [[PluginError]]> load_plugin(std::string_view path) = 0;`
- `virtual void unload_plugin(std::string_view name) = 0;`
- `virtual std::vector<std::shared_ptr<[[IPlugin]]>> get_active_plugins() const = 0;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T08:05:14.221274+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/plugin_manager.hpp`

<!-- AUTO-GENERATED END -->











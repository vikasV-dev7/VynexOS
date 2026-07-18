<!-- AUTO-GENERATED START -->
# [[BasicPluginManager]]

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
- `~[[BasicPluginManager]]() override;`
- `std::expected<std::shared_ptr<[[IPlugin]]>, [[PluginError]]> load_plugin(std::string_view path) override;`
- `void unload_plugin(std::string_view name) override;`
- `std::vector<std::shared_ptr<[[IPlugin]]>> get_active_plugins() const override;`

---
### Generator Metadata
- **Generation Timestamp**: 2026-07-18T06:24:40.007621+00:00
- **Generator Version**: 2.1.0
- **Documentation Schema**: 1.1
- **IR Version**: 1.0
- **Parser**: RegexParser
- **Source File**: `C:/Users/User/Desktop/Git Projects/VynexOS/src/core/basic_plugin_manager.hpp`

<!-- AUTO-GENERATED END -->



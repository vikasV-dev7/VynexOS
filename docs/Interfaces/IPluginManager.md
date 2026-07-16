<!-- AUTO-GENERATED START -->
# [[IPluginManager]]

## Purpose
Interface for [[IPluginManager]].

## Namespace
`vynexos::core`

## Source Location
`C:/Users/User/Desktop/Git Projects/VynexOS/include/vynexos/core/plugin_manager.hpp`

## Responsibilities
To be documented.

## Dependencies
None (pure interface).

## Methods
- `virtual ~[[IPluginManager]]() = default`
- `virtual std::expected<std::shared_ptr<[[IPlugin]]>, [[PluginError]]> load_plugin(std::string_view path) = 0`
- `virtual void unload_plugin(std::string_view name) = 0`
- `virtual std::vector<std::shared_ptr<[[IPlugin]]>> get_active_plugins() const = 0`

## Thread Safety
Implementations must ensure thread safety if accessed across multiple sub-systems via Dependency Injection.

## Used By
See [[Composition Root]]

## Implemented By
See Services directory.

<!-- AUTO-GENERATED END -->

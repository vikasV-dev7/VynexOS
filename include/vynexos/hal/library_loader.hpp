#pragma once
#include <string_view>
#include <expected>
#include <memory>
#include <string>

namespace vynexos::hal {

enum class LibraryLoadError {
    FileNotFound,
    AccessDenied,
    InvalidFormat,
    UnknownError
};

class ISharedLibrary {
public:
    virtual ~ISharedLibrary() = default;
    
    // Retrieves a function pointer by name
    virtual void* get_symbol(std::string_view name) const = 0;
};

class ILibraryLoader {
public:
    virtual ~ILibraryLoader() = default;
    
    // Loads a shared library from the given path
    virtual std::expected<std::shared_ptr<ISharedLibrary>, LibraryLoadError> load_library(std::string_view path) = 0;
};

// Factory method to create the appropriate OS loader
std::unique_ptr<ILibraryLoader> create_library_loader();

} // namespace vynexos::hal

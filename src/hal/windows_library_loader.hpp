#pragma once
#include "vynexos/hal/library_loader.hpp"
#include <windows.h>
#include <string>

namespace vynexos::hal {

class WindowsSharedLibrary final : public ISharedLibrary {
public:
    explicit WindowsSharedLibrary(HMODULE handle);
    ~WindowsSharedLibrary() override;
    
    // Non-copyable
    WindowsSharedLibrary(const WindowsSharedLibrary&) = delete;
    WindowsSharedLibrary& operator=(const WindowsSharedLibrary&) = delete;

    void* get_symbol(std::string_view name) const override;

private:
    HMODULE m_handle{nullptr};
};

class WindowsLibraryLoader final : public ILibraryLoader {
public:
    WindowsLibraryLoader() = default;
    ~WindowsLibraryLoader() override = default;

    std::expected<std::shared_ptr<ISharedLibrary>, LibraryLoadError> load_library(std::string_view path) override;
};

} // namespace vynexos::hal

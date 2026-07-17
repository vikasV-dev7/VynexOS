#include "windows_library_loader.hpp"

namespace vynexos::hal {

WindowsSharedLibrary::WindowsSharedLibrary(HMODULE handle) : m_handle(handle) {}

WindowsSharedLibrary::~WindowsSharedLibrary() {
    if (m_handle) {
        FreeLibrary(m_handle);
    }
}

void* WindowsSharedLibrary::get_symbol(std::string_view name) const {
    if (!m_handle) return nullptr;
    
    // GetProcAddress requires a null-terminated string
    std::string null_term_name(name);
    
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
    return reinterpret_cast<void*>(GetProcAddress(m_handle, null_term_name.c_str()));
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

std::expected<std::shared_ptr<ISharedLibrary>, LibraryLoadError> WindowsLibraryLoader::load_library(std::string_view path) {
    std::string null_term_path(path);
    HMODULE handle = LoadLibraryA(null_term_path.c_str());
    
    if (!handle) {
        DWORD err = GetLastError();
        if (err == ERROR_MOD_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) {
            return std::unexpected(LibraryLoadError::FileNotFound);
        }
        if (err == ERROR_ACCESS_DENIED) {
            return std::unexpected(LibraryLoadError::AccessDenied);
        }
        if (err == ERROR_BAD_FORMAT) {
            return std::unexpected(LibraryLoadError::InvalidFormat);
        }
        return std::unexpected(LibraryLoadError::UnknownError);
    }
    
    return std::make_shared<WindowsSharedLibrary>(handle);
}

} // namespace vynexos::hal

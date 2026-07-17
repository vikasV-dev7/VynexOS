#include "posix_library_loader.hpp"
#include <dlfcn.h>
#include <errno.h>

namespace vynexos::hal {

PosixSharedLibrary::PosixSharedLibrary(void* handle) : m_handle(handle) {}

PosixSharedLibrary::~PosixSharedLibrary() {
    if (m_handle) {
        dlclose(m_handle);
    }
}

void* PosixSharedLibrary::get_symbol(std::string_view name) const {
    if (!m_handle) return nullptr;
    
    std::string null_term_name(name);
    return dlsym(m_handle, null_term_name.c_str());
}

std::expected<std::shared_ptr<ISharedLibrary>, LibraryLoadError> PosixLibraryLoader::load_library(std::string_view path) {
    std::string null_term_path(path);
    
    // Clear any existing errors
    dlerror();
    
    void* handle = dlopen(null_term_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    
    if (!handle) {
        const char* err = dlerror();
        // Since POSIX doesn't provide fine-grained error codes for dlopen, we assume generic errors 
        // based on common strings, but default to UnknownError.
        if (err != nullptr) {
            std::string err_str(err);
            if (err_str.find("cannot open shared object file: No such file or directory") != std::string::npos) {
                return std::unexpected(LibraryLoadError::FileNotFound);
            }
            if (err_str.find("Permission denied") != std::string::npos) {
                return std::unexpected(LibraryLoadError::AccessDenied);
            }
            if (err_str.find("invalid ELF header") != std::string::npos) {
                return std::unexpected(LibraryLoadError::InvalidFormat);
            }
        }
        return std::unexpected(LibraryLoadError::UnknownError);
    }
    
    return std::make_shared<PosixSharedLibrary>(handle);
}

} // namespace vynexos::hal

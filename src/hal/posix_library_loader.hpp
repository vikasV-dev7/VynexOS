#pragma once
#include "vynexos/hal/library_loader.hpp"
#include <string>

namespace vynexos::hal {

class PosixSharedLibrary final : public ISharedLibrary {
public:
    explicit PosixSharedLibrary(void* handle);
    ~PosixSharedLibrary() override;
    
    // Non-copyable
    PosixSharedLibrary(const PosixSharedLibrary&) = delete;
    PosixSharedLibrary& operator=(const PosixSharedLibrary&) = delete;

    void* get_symbol(std::string_view name) const override;

private:
    void* m_handle{nullptr};
};

class PosixLibraryLoader final : public ILibraryLoader {
public:
    PosixLibraryLoader() = default;
    ~PosixLibraryLoader() override = default;

    std::expected<std::shared_ptr<ISharedLibrary>, LibraryLoadError> load_library(std::string_view path) override;
};

} // namespace vynexos::hal

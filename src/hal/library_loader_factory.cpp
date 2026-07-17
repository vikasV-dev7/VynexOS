#include "vynexos/hal/library_loader.hpp"

#ifdef _WIN32
#include "windows_library_loader.hpp"
#else
#include "posix_library_loader.hpp"
#endif

namespace vynexos::hal {

std::unique_ptr<ILibraryLoader> create_library_loader() {
#ifdef _WIN32
    return std::make_unique<WindowsLibraryLoader>();
#else
    return std::make_unique<PosixLibraryLoader>();
#endif
}

} // namespace vynexos::hal

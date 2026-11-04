#include "startup_options.hpp"
#include <vector>

namespace vynexos::bootstrap {

namespace {
constexpr std::string_view kProfileOption = "--vfs-profile";
}

StartupParse parse_startup_options(std::span<const std::string_view> args) {
    StartupParse result;
    for (const std::string_view arg : args) {
        if (arg == kProfileOption || (arg.starts_with(kProfileOption) && arg.size() == kProfileOption.size() + 1 && arg.back() == '=')) {
            result.error = "--vfs-profile needs a value: --vfs-profile=empty or --vfs-profile=development";
            return result;
        }
        if (arg.starts_with(kProfileOption) && arg[kProfileOption.size()] == '=') {
            const std::string_view name = arg.substr(kProfileOption.size() + 1);
            const auto profile = parse_vfs_profile(name);
            if (!profile) {
                result.error = "unknown VFS profile '" + std::string(name) + "' (expected: empty, development)";
                return result;
            }
            result.options.vfs_profile = *profile;
            continue;
        }
        result.error = "unknown argument '" + std::string(arg) + "'";
        return result;
    }
    return result;
}

StartupParse parse_startup_options(int argc, const char* const* argv) {
    std::vector<std::string_view> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return parse_startup_options(args);
}

std::string startup_usage() {
    return "usage: vynex_init [--vfs-profile=empty|development]\n"
           "  --vfs-profile   initial filesystem contents (default: development)\n";
}

} // namespace vynexos::bootstrap

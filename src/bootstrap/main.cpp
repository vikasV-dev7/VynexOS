#include "composition_root.hpp"
#include <iostream>

int main(int argc, char** argv) {
    const auto startup = vynexos::bootstrap::parse_startup_options(argc, argv);
    if (!startup.ok()) {
        std::cerr << "vynex_init: " << startup.error << '\n' << vynexos::bootstrap::startup_usage();
        return 2;
    }

    vynexos::bootstrap::CompositionRoot root(startup.options);

    root.initialize();
    root.run();
    root.shutdown();

    return 0;
}

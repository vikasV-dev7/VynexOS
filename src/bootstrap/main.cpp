#include "composition_root.hpp"

int main() {
    vynexos::bootstrap::CompositionRoot root;
    
    root.initialize();
    root.run();
    root.shutdown();
    
    return 0;
}

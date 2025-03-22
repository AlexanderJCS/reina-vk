#include <iostream>
#include "Reina.h"


int main() {
    try {
        Reina reina{};
        reina.renderLoop();
        // Reina object is destroyed automatically as it goes out of scope

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}

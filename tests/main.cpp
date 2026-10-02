// Runs every registered TEST. Build target: keymapper_tests.
#include "harness.h"

int main() {
    for (const auto& c : testing::cases()) {
        const int before = testing::failures();
        c.run();
        if (testing::failures() != before) std::cerr << "  ^ in test " << c.name << "\n";
    }
    std::cout << (testing::checks() - testing::failures()) << "/" << testing::checks() << " checks passed in "
              << testing::cases().size() << " tests\n";
    return testing::failures() == 0 ? 0 : 1;
}

// The Windows layer's built-in actions. Built on Windows only, like the rest of tests/windows/.
#include "harness.h"
#include "helpers.h"

#include "keycodes.h"

TEST(windowsSwitchInputSourceSendsControlShift) {
    auto lib = parse(sourceFile("config/actions.windows.txt"));
    const auto& steps = lib.cfg.actions["switch-input-source"];
    // Windows switches the input language on Ctrl+Shift (not Win+Space): press both modifiers, then release.
    CHECK(steps.size() == 1 && steps[0].mods == ModCtrl && steps[0].key == key::LShift);
}

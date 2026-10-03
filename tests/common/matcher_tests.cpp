// Chord matching and how an action expands into key events.
#include "harness.h"
#include "helpers.h"

#include "keycodes.h"
#include "matcher.h"

TEST(altQBindingMatchesOnlyExactChord) {
    auto p = parse(kAltQ);
    const auto& list = p.cfg.base.chords;
    CHECK(findBinding(list, ModAlt, 'Q') != nullptr);
    CHECK(findBinding(list, ModAlt | ModShift, 'Q') == nullptr);  // extra modifier
    CHECK(findBinding(list, 0, 'Q') == nullptr);                  // no modifier
    CHECK(findBinding(list, ModCtrl, 'Q') == nullptr);            // wrong modifier
    CHECK(findBinding(list, ModAlt, 'W') == nullptr);             // wrong key
}

TEST(altQSendsTabWhileAltStaysHeld) {
    auto p = parse(kAltQ);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr);
    if (!b) return;

    // Alt is physically held (that's how Alt+Q was pressed), so only Tab is tapped:
    // the Windows switcher stays open for as long as the user keeps Alt down.
    CHECK(same(expand(b->out, {key::LAlt}), {{key::Tab, false}, {key::Tab, true}}));
    // Right Alt works the same.
    CHECK(same(expand(b->out, {key::RAlt}), {{key::Tab, false}, {key::Tab, true}}));
}

TEST(expandPressesMissingModifiers) {
    Steps s = {{ModAlt, key::Tab}};
    CHECK(same(expand(s, {}), {{key::LAlt, false}, {key::Tab, false}, {key::Tab, true}, {key::LAlt, true}}));
}

TEST(expandSwapsOutPhysicalModifiers) {
    // Output wants Ctrl+C but the user is physically holding Alt: release Alt, send, restore Alt.
    Steps s = {{ModCtrl, 'C'}};
    CHECK(same(expand(s, {key::LAlt}),
               {{key::LAlt, true}, {key::LControl, false}, {'C', false}, {'C', true}, {key::LControl, true}, {key::LAlt, false}}));
}

TEST(expandRunsStepsInOrder) {
    Steps s = {{ModCtrl, 'C'}, {ModCtrl, 'V'}};
    CHECK(same(expand(s, {}),
               {{key::LControl, false}, {'C', false}, {'C', true}, {key::LControl, true},
                {key::LControl, false}, {'V', false}, {'V', true}, {key::LControl, true}}));
}

TEST(modifierKeyDetection) {
    CHECK(modBit(key::LAlt) == ModAlt);
    CHECK(modBit(key::RAlt) == ModAlt);
    CHECK(modBit(key::LControl) == ModCtrl);
    CHECK(modBit(key::RShift) == ModShift);
    CHECK(modBit(key::LWin) == ModWin);
    CHECK(modBit('Q') == 0);
}

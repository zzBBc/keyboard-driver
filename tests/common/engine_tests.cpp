// The engine: what to do with each physical key event (the platform-independent part of the hook).
#include "harness.h"
#include "helpers.h"

#include "engine.h"
#include "keycodes.h"

TEST(engineRemapsAKeyAndItsRepeatAndRelease) {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    auto down = press(e, "K1", key::Tab);
    CHECK(down.swallow && same(down.send, {{key::CapsLock, false}}));
    auto repeat = press(e, "K1", key::Tab);  // auto-repeat
    CHECK(repeat.swallow && same(repeat.send, {{key::CapsLock, false}}));
    auto up = press(e, "K1", key::Tab, true);
    CHECK(up.swallow && same(up.send, {{key::CapsLock, true}}));
}

TEST(engineLeavesOtherKeysAlone) {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    auto a = press(e, "K1", 'A');
    CHECK(!a.swallow && a.send.empty());
    auto aUp = press(e, "K1", 'A', true);
    CHECK(!aUp.swallow && aUp.send.empty());
}

TEST(engineLayerKeyIsSwallowedAndRemapsWhileHeld) {
    Engine e;
    e.setConfig("", cfgOf("[layer CapsLock]\nh = left\n"));
    auto layer = press(e, "K1", key::CapsLock);
    CHECK(layer.swallow && layer.send.empty());
    auto h = press(e, "K1", 'H');
    CHECK(h.swallow && same(h.send, {{key::Left, false}}));
    auto hUp = press(e, "K1", 'H', true);
    CHECK(hUp.swallow && same(hUp.send, {{key::Left, true}}));
    auto layerUp = press(e, "K1", key::CapsLock, true);
    CHECK(layerUp.swallow && layerUp.send.empty());
    auto after = press(e, "K1", 'H');  // layer released: h is a normal key again
    CHECK(!after.swallow && after.send.empty());
}

TEST(engineReleasesWhatItPressedEvenAfterTheLayerIsGone) {
    Engine e;
    e.setConfig("", cfgOf("[layer CapsLock]\nh = left\n"));
    press(e, "K1", key::CapsLock);
    press(e, "K1", 'H');
    press(e, "K1", key::CapsLock, true);          // layer key released first
    auto hUp = press(e, "K1", 'H', true);          // h must still release Left, not stay stuck
    CHECK(hUp.swallow && same(hUp.send, {{key::Left, true}}));
}

TEST(engineChordRunsActionWhileModifierHeld) {
    Engine e;
    e.setConfig("", cfgOf("[action sw]\nalt+tab\nalt+q = @sw\n"));
    auto alt = press(e, "K1", key::LAlt);
    CHECK(!alt.swallow);  // modifiers pass through so apps still see them
    auto q = press(e, "K1", 'Q');
    CHECK(q.swallow && same(q.send, {{key::Tab, false}, {key::Tab, true}}));  // Alt is already held
    auto qRepeat = press(e, "K1", 'Q');
    CHECK(qRepeat.swallow && q.send.size() == 2 && qRepeat.send.empty());     // repeat swallowed, not re-run
    auto qUp = press(e, "K1", 'Q', true);
    CHECK(qUp.swallow && qUp.send.empty());
}

TEST(engineChordNeedsExactlyThoseModifiers) {
    Engine e;
    e.setConfig("", cfgOf("[action sw]\nalt+tab\nalt+q = @sw\n"));
    auto bare = press(e, "K1", 'Q');                // no Alt
    CHECK(!bare.swallow && bare.send.empty());
    press(e, "K1", 'Q', true);
    press(e, "K1", key::LAlt);
    press(e, "K1", key::LShift);
    auto extra = press(e, "K1", 'Q');               // Alt+Shift+Q is a different combo
    CHECK(!extra.swallow && extra.send.empty());
}

TEST(engineChordSwapsPhysicalModifiers) {
    Engine e;
    e.setConfig("", cfgOf("alt+w = ctrl+c\n"));
    press(e, "K1", key::LAlt);
    auto w = press(e, "K1", 'W');
    CHECK(w.swallow);
    CHECK(same(w.send, {{key::LAlt, true}, {key::LControl, false}, {'C', false}, {'C', true}, {key::LControl, true}, {key::LAlt, false}}));
}

TEST(engineChordReleasesOnlyTheModifierTheOutputDoesNotWant) {
    Engine e;
    e.setConfig("", cfgOf("control+shift+cmd+4 = shift+cmd+s\n"));
    press(e, "K1", key::LControl);
    press(e, "K1", key::LShift);
    press(e, "K1", key::LWin);
    auto four = press(e, "K1", '4');
    CHECK(four.swallow);
    CHECK(same(four.send, {{key::LControl, true}, {'S', false}, {'S', true}, {key::LControl, false}}));
}

TEST(engineChangingWhatAnActionSendsDoesNotBindItsKeys) {
    Engine e;  // the built-in action sends control+shift+cmd+4; changing that gives it no shortcut
    e.setConfig("", cfgOf("[action screenshot-region-clipboard]\nshift+cmd+s\n"));
    press(e, "K1", key::LControl);
    press(e, "K1", key::LShift);
    press(e, "K1", key::LWin);
    auto four = press(e, "K1", '4');
    CHECK(!four.swallow && four.send.empty());
}

TEST(engineKeepsConfigPerKeyboard) {
    Engine e;
    e.setConfig("", cfgOf(""));
    e.setConfig("A", cfgOf("tab = capslock\n"));
    CHECK(press(e, "A", key::Tab).swallow);
    press(e, "A", key::Tab, true);
    CHECK(!press(e, "B", key::Tab).swallow);       // B has no config of its own: the (empty) default
}

TEST(engineKeepsLayerAndModifierStatePerKeyboard) {
    Engine e;
    e.setConfig("", cfgOf("[layer CapsLock]\nh = left\n[action sw]\nalt+tab\nalt+q = @sw\n"));
    press(e, "A", key::CapsLock);                   // layer held on keyboard A...
    CHECK(!press(e, "B", 'H').swallow);             // ...does not affect keyboard B
    press(e, "A", key::LAlt);                       // Alt held on A...
    CHECK(!press(e, "B", 'Q').swallow);             // ...is not Alt+Q on B
}

TEST(engineReleaseFindsTheKeyEvenIfAttributedToAnotherKeyboard) {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    press(e, "A", key::Tab);
    auto up = press(e, "B", key::Tab, true);        // release arrives attributed to B (a wrong guess)
    CHECK(up.swallow && same(up.send, {{key::CapsLock, true}}));
}

TEST(engineConfigChangeDoesNotLeaveKeysStuck) {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    press(e, "K1", key::Tab);
    e.setConfig("", cfgOf(""));                     // config changes while the key is down
    auto up = press(e, "K1", key::Tab, true);
    CHECK(up.swallow && same(up.send, {{key::CapsLock, true}}));
    CHECK(!press(e, "K1", key::Tab).swallow);       // and the new (empty) config applies from now on
}

TEST(engineNullConfigRemovesAKeyboardsOwn) {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    e.setConfig("A", cfgOf(""));                    // A overrides the default with nothing
    CHECK(!press(e, "A", key::Tab).swallow);
    e.setConfig("A", nullptr);                      // remove A's own config: back to the default
    CHECK(press(e, "A", key::Tab).swallow);
}

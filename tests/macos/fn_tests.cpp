// The fn modifier (the Fn/Globe key): only macOS can send it, so its tests are built on macOS only,
// like the rest of tests/macos/. How the macOS layer sends it is in mac_input_tests.cpp.
#include "harness.h"
#include "helpers.h"

#include "keycodes.h"
#include "matcher.h"

TEST(fnCanBeSentButIsNotAShortcut) {
    // keymapper never sees Fn held, so a shortcut with it could never fire: the config says so.
    auto sent = parse("[action snap]\nfn+control+left\n\nalt+left = fn+control+left\n");
    for (const auto& e : sent.errors) std::cerr << "  parse error: " << e << "\n";
    CHECK(sent.ok && sent.errors.empty());
    CHECK(sent.cfg.actions["snap"][0].mods == (ModFn | ModCtrl));
    const Binding* b = findBinding(sent.cfg.base.chords, ModAlt, key::Left);
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == (ModFn | ModCtrl));

    auto from = parse("fn+left = home\n");
    CHECK(!from.ok && from.errors.size() == 1 && from.errors[0].find("fn can only be sent") != std::string::npos);
    auto shortcut = parse("[action snap]\nshortcut: fn+q\nfn+control+left\n");
    CHECK(!shortcut.ok);
}

TEST(expandPressesFnFirstAndReleasesItLast) {
    // fn+control+left (macOS window tiling): Fn goes down before Control, like a person presses it.
    auto p = parse("[action snap]\nfn+control+left\n");
    CHECK(p.ok && p.cfg.actions["snap"].size() == 1 && p.cfg.actions["snap"][0].mods == (ModFn | ModCtrl));
    CHECK(same(expand(p.cfg.actions["snap"], {}),
               {{key::Fn, false}, {key::LControl, false}, {key::Left, false}, {key::Left, true},
                {key::Fn, true}, {key::LControl, true}}));
    CHECK(modBit(key::Fn) == ModFn);
}

TEST(macActionListTilesWithFn) {
    // macOS 15's window tiling shortcuts hold Fn: snap-left is Fn+Control+Left.
    auto mac = parse(sourceFile("config/actions.macos.txt"));
    CHECK(mac.ok);
    CHECK(mac.cfg.actions["snap-left"].size() == 1 && mac.cfg.actions["snap-left"][0].mods == (ModFn | ModCtrl));
    CHECK(mac.cfg.actions["snap-left"][0].key == key::Left);
    CHECK(mac.cfg.actions["center-window"].size() == 1 && mac.cfg.actions["center-window"][0].mods == (ModFn | ModCtrl));
}

TEST(fnAloneIsATapOfTheFnKey) {
    // A step of just "fn" presses and releases Fn (Globe) on its own, as a person taps it.
    auto p = parse("[action globe]\nfn\n\nalt+g = fn\n");
    for (const auto& e : p.errors) std::cerr << "  parse error: " << e << "\n";
    CHECK(p.ok && p.errors.empty());
    CHECK(p.cfg.actions["globe"].size() == 1 && p.cfg.actions["globe"][0].mods == 0);
    CHECK(p.cfg.actions["globe"][0].key == key::Fn);
    CHECK(same(expand(p.cfg.actions["globe"], {}), {{key::Fn, false}, {key::Fn, true}}));
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'G');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].key == key::Fn);
    CHECK(!parse("fn = a\n").ok);  // still not a shortcut
}

TEST(macSwitchInputSourceTapsFnLikeTheGlobeKey) {
    // switch-input-source does what pressing Fn (Globe) does: Control+Shift lets go of the
    // modifiers and taps Fn alone, then puts back the one still held.
    auto lib = parse(sourceFile("config/actions.macos.txt"));
    CHECK(lib.ok);
    CHECK(same(expand(lib.cfg.actions["switch-input-source"], {}), {{key::Fn, false}, {key::Fn, true}}));
    auto p = parseWithLibrary("", lib.cfg);
    CHECK(p.ok);
    Engine e;
    e.setConfig("", std::make_shared<Config>(p.cfg));
    press(e, "K1", key::LControl);
    press(e, "K1", key::LShift);
    auto up = press(e, "K1", key::LShift, true);
    CHECK(up.swallow);
    CHECK(same(up.send, {{key::LShift, true}, {key::LControl, true}, {key::Fn, false}, {key::Fn, true},
                         {key::LControl, false}}));
}

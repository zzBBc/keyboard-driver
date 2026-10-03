// Config parsing: combos, actions, descriptions, categories, the built-in library, default shortcuts and clearing them.
#include "harness.h"
#include "helpers.h"

#include "config_loader.h"
#include "key_names.h"
#include "keycodes.h"
#include "matcher.h"

TEST(altQRunsSwitchWindowAction) {
    auto p = parse(kAltQ);
    for (const auto& e : p.errors) std::cerr << "  parse error: " << e << "\n";
    CHECK(p.ok);
    CHECK(p.errors.empty());

    // The action is stored under its name as one step: Alt+Tab.
    CHECK(p.cfg.actions.count("switch-window") == 1);
    const Steps& steps = p.cfg.actions["switch-window"];
    CHECK(steps.size() == 1);
    CHECK(steps[0].mods == ModAlt);
    CHECK(steps[0].key == key::Tab);

    // The binding lives in the base chords (not the plain key map) and carries the action's steps.
    CHECK(p.cfg.base.map.empty());
    CHECK(p.cfg.base.chords.size() == 1);
    const Binding& b = p.cfg.base.chords[0];
    CHECK(b.from.mods == ModAlt);
    CHECK(b.from.key == 'Q');
    CHECK(b.out.size() == 1 && b.out[0].mods == ModAlt && b.out[0].key == key::Tab);
}

TEST(commaSeparatesSteps) {
    auto p = parse("[action copy-paste]\nctrl+c, ctrl+v\n");
    CHECK(p.ok);
    const Steps& s = p.cfg.actions["copy-paste"];
    CHECK(s.size() == 2);
    CHECK(s.size() == 2 && s[0].mods == ModCtrl && s[0].key == 'C' && s[1].mods == ModCtrl && s[1].key == 'V');
}

TEST(unknownActionIsAnError) {
    auto p = parse("alt+q = @nope\n");
    CHECK(!p.ok);
    CHECK(p.errors.size() == 1);
    CHECK(!p.errors.empty() && p.errors[0].find("line 1") != std::string::npos);
    CHECK(!p.errors.empty() && p.errors[0].find("nope") != std::string::npos);
}

TEST(actionMayBeDefinedAfterItsBinding) {
    auto p = parse("alt+q = @late\n[action late]\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1 && p.cfg.base.chords[0].out.size() == 1);
}

TEST(plainKeyMappingsStayInTheKeyMap) {
    auto p = parse("tab = capslock\n[layer CapsLock]\nh = left\n");
    CHECK(p.ok);
    CHECK(p.cfg.base.map.size() == 1);
    CHECK(p.cfg.base.chords.empty());
    CHECK(p.cfg.layers.size() == 1 && p.cfg.layers[0].map.size() == 1 && p.cfg.layers[0].chords.empty());
}

TEST(chordBindingsWorkInsideALayer) {
    auto p = parse("[action a]\nalt+tab\n[layer CapsLock]\nalt+h = @a\n");
    CHECK(p.ok);
    CHECK(p.cfg.layers.size() == 1 && p.cfg.layers[0].chords.size() == 1);
    CHECK(p.cfg.base.chords.empty());
}

TEST(comboOutputWithoutAction) {
    auto p = parse("alt+w = ctrl+shift+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1);
    CHECK(p.cfg.base.chords[0].out.size() == 1 && p.cfg.base.chords[0].out[0].mods == (ModCtrl | ModShift));
}

TEST(badChordIsAnError) {
    CHECK(!parse("foo+q = a\n").ok);                    // unknown modifier
    CHECK(!parse("alt+ = a\n").ok);                     // missing key
    CHECK(!parse("[action x]\nalt+nokey\n").ok);        // unknown key in action
    CHECK(!parse("[action bad name]\nalt+tab\n").ok);   // invalid action name
    CHECK(!parse("[action x]\nalt+tab\n[action x]\nctrl+c\n").ok);  // duplicate
}

TEST(descriptionIsParsed) {
    auto p = parse("[action a]\ndescription: Switch to the next window\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionInfo["a"] == "Switch to the next window");
    CHECK(p.cfg.actions["a"].size() == 1);  // the description line is not a step
}

TEST(libraryActionIsUsedWhenNotDefined) {
    auto lib = parse("[action switch-window]\ndescription: Switch window\nalt+tab\n");
    CHECK(lib.ok);

    Config cfg;
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+q = @switch-window\n", cfg, errors, &lib.cfg));
    CHECK(cfg.base.chords.size() == 1);
    CHECK(cfg.base.chords.size() == 1 && cfg.base.chords[0].out.size() == 1 && cfg.base.chords[0].out[0].key == key::Tab);

    Config alone;
    std::vector<std::string> errors2;
    CHECK(!parseConfig("alt+q = @switch-window\n", alone, errors2));  // no library: still unknown
}

TEST(userActionOverridesLibrary) {
    auto lib = parse("[action switch-window]\nalt+tab\n");
    Config cfg;
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+q = @switch-window\n[action switch-window]\nctrl+tab\n", cfg, errors, &lib.cfg));
    CHECK(cfg.base.chords.size() == 1 && cfg.base.chords[0].out.size() == 1);
    CHECK(cfg.base.chords[0].out[0].mods == ModCtrl);
}

TEST(defaultShortcutIsParsedInTheLibrary) {
    auto lib = parse(kLibWithDefault);
    CHECK(lib.ok);
    CHECK(lib.cfg.actionShortcuts.count("switch-window") == 1);
    const auto& combos = lib.cfg.actionShortcuts["switch-window"];
    CHECK(combos.size() == 1);
    CHECK(combos.size() == 1 && combos[0].mods == ModAlt && combos[0].key == 'Q');
    CHECK(lib.cfg.actionShortcuts.count("copy") == 0);  // no shortcut declared
}

TEST(severalDefaultShortcutsAndBadOnes) {
    auto two = parse("[action a]\nshortcut: alt+q\nshortcut: alt+w\nalt+tab\n");
    CHECK(two.ok);
    CHECK(two.cfg.actionShortcuts["a"].size() == 2);
    CHECK(!parse("[action a]\nshortcut: alt+nokey\nalt+tab\n").ok);  // unknown key
    CHECK(!parse("[action a]\nshortcut: foo+q\nalt+tab\n").ok);      // unknown modifier
}

TEST(defaultShortcutIsBoundWithoutAnyUserConfig) {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("", lib.cfg);
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr);
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModAlt && b->out[0].key == key::Tab);
}

TEST(noLibraryMeansNoDefaults) {
    auto p = parse("");
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.empty());
}

TEST(userBindingReplacesTheDefault) {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+q = @copy\n", lib.cfg);
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1);  // one binding for alt+q, the user's
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModCtrl && b->out[0].key == 'C');
}

TEST(defaultShortcutCanBeCleared) {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+q = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(p.errors.empty());
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') == nullptr);
    CHECK(p.cfg.base.chords.empty());
}

TEST(clearingOneDefaultKeepsTheOthers) {
    auto lib = parse("[action a]\nshortcut: alt+q\nalt+tab\n[action b]\nshortcut: alt+w\nctrl+c\n");
    auto p = parseWithLibrary("alt+q = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') == nullptr);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'W') != nullptr);
}

TEST(clearedShortcutCanBeRebound) {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+q = none\nalt+q = @copy\n", lib.cfg);  // last line wins
    CHECK(p.ok);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].key == 'C');
}

TEST(clearingAnUnboundComboIsHarmless) {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+z = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') != nullptr);  // default untouched
}

TEST(clearWorksWithoutALibraryToo) {
    auto p = parse("alt+q = @switch\n[action switch]\nalt+tab\n");
    CHECK(p.ok && p.cfg.base.chords.size() == 1);
    auto q = parse("alt+q = @switch\nalt+q = none\n[action switch]\nalt+tab\n");
    CHECK(q.ok);
    CHECK(q.cfg.base.chords.empty());  // a user binding is cleared the same way
}

TEST(clearIsPerScope) {
    // Clearing alt+q in a layer doesn't touch the base default.
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("[layer CapsLock]\nalt+q = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') != nullptr);
}

TEST(userRedefinedActionKeepsItsDefaultShortcut) {
    // The user redefines switch-window; the default combo now runs the user's steps.
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("[action switch-window]\nctrl+tab\n", lib.cfg);
    CHECK(p.ok);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModCtrl);
}

TEST(mediaKeyActionIsBindable) {
    auto lib = parse("[action volume-up]\ndescription: Raise the volume\nvolumeup\n");
    CHECK(lib.ok);
    CHECK(lib.cfg.actions["volume-up"].size() == 1 && lib.cfg.actions["volume-up"][0].key == key::VolumeUp);
    CHECK(lib.cfg.actionShortcuts.count("volume-up") == 0);  // no default combo

    Config cfg;
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+up = @volume-up\n", cfg, errors, &lib.cfg));
    const Binding* b = findBinding(cfg.base.chords, ModAlt, key::Up);
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == 0 && b->out[0].key == key::VolumeUp);
    CHECK(cfg.base.chords.size() == 1);  // and nothing else got bound by default
}

TEST(mediaKeyWorksInComboOutput) {
    auto p = parse("alt+down = volumedown\n");  // a media key as the plain target of a combo
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1 && p.cfg.base.chords[0].out[0].key == key::VolumeDown);
}

TEST(categoryIsParsed) {
    auto p = parse("[action a]\ndescription: A\ncategory: Windows and desktops\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionCategory["a"] == "Windows and desktops");
    CHECK(p.cfg.actions["a"].size() == 1);  // the category line is not a step
}

TEST(categoryIsOptional) {
    auto p = parse("[action a]\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionCategory.count("a") == 0);
}

TEST(categoryAndOtherLinesCanBeInAnyOrder) {
    auto p = parse("[action a]\nalt+tab\nshortcut: alt+q\ncategory: Windows\ndescription: Switch\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionCategory["a"] == "Windows");
    CHECK(p.cfg.actionInfo["a"] == "Switch");
    CHECK(p.cfg.actionShortcuts["a"].size() == 1);
}

TEST(actionOrderFollowsTheFile) {
    auto p = parse("[action zeta]\nalt+tab\n[action alpha]\nctrl+c\n[action mid]\nctrl+v\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionOrder.size() == 3);
    CHECK(p.cfg.actionOrder.size() == 3 && p.cfg.actionOrder[0] == "zeta" && p.cfg.actionOrder[1] == "alpha" && p.cfg.actionOrder[2] == "mid");
}

TEST(shippedActionListsParse) {
    // Both OS lists load without errors, and the Mac one reuses the Windows names where it can.
    auto win = parse(sourceFile("config/actions.windows.txt"));
    auto mac = parse(sourceFile("config/actions.macos.txt"));
    for (const auto& e : win.errors) std::cerr << "  actions.windows.txt: " << e << "\n";
    for (const auto& e : mac.errors) std::cerr << "  actions.macos.txt: " << e << "\n";
    CHECK(win.ok && win.errors.empty() && win.cfg.actions.size() == 55);
    CHECK(mac.ok && mac.errors.empty() && mac.cfg.actions.size() == 62);
    for (const char* name : {"switch-window", "close-window", "task-view", "copy", "undo", "new-tab"})
        CHECK(win.cfg.actions.count(name) == 1 && mac.cfg.actions.count(name) == 1);
    CHECK(mac.cfg.actions["copy"].size() == 1 && mac.cfg.actions["copy"][0].mods == ModWin);  // Cmd+C
}

TEST(androidActionListParses) {
    auto android = parse(sourceFile("config/actions.android.txt"));
    for (const auto& e : android.errors) std::cerr << "  actions.android.txt: " << e << "\n";
    CHECK(android.ok && android.errors.empty() && android.cfg.actions.size() == 8);
    CHECK(android.cfg.actions.count("back") == 1 && android.cfg.actions.count("screenshot") == 1);
}

TEST(macModifierNamesMeanTheSameModifiers) {
    // cmd/command = win, option/opt = alt, control = ctrl, on every OS.
    auto p = parse("cmd+option+control+shift+q = a\ncommand+opt+w = b\n");
    for (const auto& e : p.errors) std::cerr << "  parse error: " << e << "\n";
    CHECK(p.ok && p.errors.empty());
    CHECK(findBinding(p.cfg.base.chords, ModWin | ModAlt | ModCtrl | ModShift, 'Q') != nullptr);
    CHECK(findBinding(p.cfg.base.chords, ModWin | ModAlt, 'W') != nullptr);
    CHECK(!parse("super+q = a\n").ok);  // still an unknown modifier
}

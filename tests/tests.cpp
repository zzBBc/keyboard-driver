// Minimal test runner (no external dependencies): run keymapper_tests.exe, exit code 0 = pass.
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "config_loader.h"
#include "key_names.h"
#include "matcher.h"

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failures;                                                             \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
        }                                                                             \
    } while (0)

struct Parsed {
    Config cfg;
    std::vector<std::string> errors;
    bool ok;
};

Parsed parse(const std::string& text) {
    Parsed p;
    p.ok = parseConfig(text, p.cfg, p.errors);
    return p;
}

bool same(const std::vector<KeyEvent>& a, const std::vector<KeyEvent>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].vk != b[i].vk || a[i].up != b[i].up) return false;
    return true;
}

const char* kAltQ =
    "[action switch-window]\n"
    "alt+tab\n"
    "\n"
    "alt+q = @switch-window\n";

// ---- the case: an action named switch-window (Alt+Tab) bound to Alt+Q ----

void altQRunsSwitchWindowAction() {
    auto p = parse(kAltQ);
    for (const auto& e : p.errors) std::cerr << "  parse error: " << e << "\n";
    CHECK(p.ok);
    CHECK(p.errors.empty());

    // The action is stored under its name as one step: Alt+Tab.
    CHECK(p.cfg.actions.count("switch-window") == 1);
    const Steps& steps = p.cfg.actions["switch-window"];
    CHECK(steps.size() == 1);
    CHECK(steps[0].mods == ModAlt);
    CHECK(steps[0].key == VK_TAB);

    // The binding lives in the base chords (not the plain key map) and carries the action's steps.
    CHECK(p.cfg.base.map.empty());
    CHECK(p.cfg.base.chords.size() == 1);
    const Binding& b = p.cfg.base.chords[0];
    CHECK(b.from.mods == ModAlt);
    CHECK(b.from.key == 'Q');
    CHECK(b.out.size() == 1 && b.out[0].mods == ModAlt && b.out[0].key == VK_TAB);
}

void altQBindingMatchesOnlyExactChord() {
    auto p = parse(kAltQ);
    const auto& list = p.cfg.base.chords;
    CHECK(findBinding(list, ModAlt, 'Q') != nullptr);
    CHECK(findBinding(list, ModAlt | ModShift, 'Q') == nullptr);  // extra modifier
    CHECK(findBinding(list, 0, 'Q') == nullptr);                  // no modifier
    CHECK(findBinding(list, ModCtrl, 'Q') == nullptr);            // wrong modifier
    CHECK(findBinding(list, ModAlt, 'W') == nullptr);             // wrong key
}

void altQSendsTabWhileAltStaysHeld() {
    auto p = parse(kAltQ);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr);
    if (!b) return;

    // Alt is physically held (that's how Alt+Q was pressed), so only Tab is tapped:
    // the Windows switcher stays open for as long as the user keeps Alt down.
    CHECK(same(expand(b->out, {VK_LMENU}), {{VK_TAB, false}, {VK_TAB, true}}));
    // Right Alt works the same.
    CHECK(same(expand(b->out, {VK_RMENU}), {{VK_TAB, false}, {VK_TAB, true}}));
}

// ---- expansion of other shapes ----

void expandPressesMissingModifiers() {
    Steps s = {{ModAlt, VK_TAB}};
    CHECK(same(expand(s, {}), {{VK_LMENU, false}, {VK_TAB, false}, {VK_TAB, true}, {VK_LMENU, true}}));
}

void expandSwapsOutPhysicalModifiers() {
    // Output wants Ctrl+C but the user is physically holding Alt: release Alt, send, restore Alt.
    Steps s = {{ModCtrl, 'C'}};
    CHECK(same(expand(s, {VK_LMENU}),
               {{VK_LMENU, true}, {VK_LCONTROL, false}, {'C', false}, {'C', true}, {VK_LCONTROL, true}, {VK_LMENU, false}}));
}

void expandRunsStepsInOrder() {
    Steps s = {{ModCtrl, 'C'}, {ModCtrl, 'V'}};
    CHECK(same(expand(s, {}),
               {{VK_LCONTROL, false}, {'C', false}, {'C', true}, {VK_LCONTROL, true},
                {VK_LCONTROL, false}, {'V', false}, {'V', true}, {VK_LCONTROL, true}}));
}

// ---- parser ----

void commaSeparatesSteps() {
    auto p = parse("[action copy-paste]\nctrl+c, ctrl+v\n");
    CHECK(p.ok);
    const Steps& s = p.cfg.actions["copy-paste"];
    CHECK(s.size() == 2);
    CHECK(s.size() == 2 && s[0].mods == ModCtrl && s[0].key == 'C' && s[1].mods == ModCtrl && s[1].key == 'V');
}

void unknownActionIsAnError() {
    auto p = parse("alt+q = @nope\n");
    CHECK(!p.ok);
    CHECK(p.errors.size() == 1);
    CHECK(!p.errors.empty() && p.errors[0].find("line 1") != std::string::npos);
    CHECK(!p.errors.empty() && p.errors[0].find("nope") != std::string::npos);
}

void actionMayBeDefinedAfterItsBinding() {
    auto p = parse("alt+q = @late\n[action late]\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1 && p.cfg.base.chords[0].out.size() == 1);
}

void plainKeyMappingsStayInTheKeyMap() {
    auto p = parse("tab = capslock\n[layer CapsLock]\nh = left\n");
    CHECK(p.ok);
    CHECK(p.cfg.base.map.size() == 1);
    CHECK(p.cfg.base.chords.empty());
    CHECK(p.cfg.layers.size() == 1 && p.cfg.layers[0].map.size() == 1 && p.cfg.layers[0].chords.empty());
}

void chordBindingsWorkInsideALayer() {
    auto p = parse("[action a]\nalt+tab\n[layer CapsLock]\nalt+h = @a\n");
    CHECK(p.ok);
    CHECK(p.cfg.layers.size() == 1 && p.cfg.layers[0].chords.size() == 1);
    CHECK(p.cfg.base.chords.empty());
}

void comboOutputWithoutAction() {
    auto p = parse("alt+w = ctrl+shift+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1);
    CHECK(p.cfg.base.chords[0].out.size() == 1 && p.cfg.base.chords[0].out[0].mods == (ModCtrl | ModShift));
}

void badChordIsAnError() {
    CHECK(!parse("foo+q = a\n").ok);                    // unknown modifier
    CHECK(!parse("alt+ = a\n").ok);                     // missing key
    CHECK(!parse("[action x]\nalt+nokey\n").ok);        // unknown key in action
    CHECK(!parse("[action bad name]\nalt+tab\n").ok);   // invalid action name
    CHECK(!parse("[action x]\nalt+tab\n[action x]\nctrl+c\n").ok);  // duplicate
}

void modifierKeyDetection() {
    CHECK(modBit(VK_LMENU) == ModAlt);
    CHECK(modBit(VK_RMENU) == ModAlt);
    CHECK(modBit(VK_LCONTROL) == ModCtrl);
    CHECK(modBit(VK_RSHIFT) == ModShift);
    CHECK(modBit(VK_LWIN) == ModWin);
    CHECK(modBit('Q') == 0);
}

// ---- action descriptions and the built-in library ----

void descriptionIsParsed() {
    auto p = parse("[action a]\ndescription: Switch to the next window\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionInfo["a"] == "Switch to the next window");
    CHECK(p.cfg.actions["a"].size() == 1);  // the description line is not a step
}

void libraryActionIsUsedWhenNotDefined() {
    auto lib = parse("[action switch-window]\ndescription: Switch window\nalt+tab\n");
    CHECK(lib.ok);

    Config cfg;
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+q = @switch-window\n", cfg, errors, &lib.cfg));
    CHECK(cfg.base.chords.size() == 1);
    CHECK(cfg.base.chords.size() == 1 && cfg.base.chords[0].out.size() == 1 && cfg.base.chords[0].out[0].key == VK_TAB);

    Config alone;
    std::vector<std::string> errors2;
    CHECK(!parseConfig("alt+q = @switch-window\n", alone, errors2));  // no library: still unknown
}

void userActionOverridesLibrary() {
    auto lib = parse("[action switch-window]\nalt+tab\n");
    Config cfg;
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+q = @switch-window\n[action switch-window]\nctrl+tab\n", cfg, errors, &lib.cfg));
    CHECK(cfg.base.chords.size() == 1 && cfg.base.chords[0].out.size() == 1);
    CHECK(cfg.base.chords[0].out[0].mods == ModCtrl);
}

void keyNameFromVk() {
    CHECK(nameFromVk(VK_TAB) == "tab");
    CHECK(nameFromVk(VK_ESCAPE) == "esc");  // shortest of esc/escape
    CHECK(nameFromVk(VK_RETURN) == "enter");
    CHECK(nameFromVk(VK_OEM_PERIOD) == ".");
    CHECK(nameFromVk('Q') == "q");
    CHECK(nameFromVk(VK_F4) == "f4");
    CHECK(nameFromVk(0x07).empty());  // undefined key
}

// ---- default shortcuts of built-in actions, and clearing them ----
//
// A library action may declare `shortcut: alt+q` lines. Those combos are bound by default in the
// base scope. A config can rebind a default (`alt+q = @copy`) or clear it (`alt+q = none`).

const char* kLibWithDefault =
    "[action switch-window]\n"
    "description: Switch window\n"
    "shortcut: alt+q\n"
    "alt+tab\n"
    "\n"
    "[action copy]\n"
    "ctrl+c\n";

Parsed parseWithLibrary(const std::string& text, const Config& lib) {
    Parsed p;
    p.ok = parseConfig(text, p.cfg, p.errors, &lib);
    return p;
}

void defaultShortcutIsParsedInTheLibrary() {
    auto lib = parse(kLibWithDefault);
    CHECK(lib.ok);
    CHECK(lib.cfg.actionShortcuts.count("switch-window") == 1);
    const auto& combos = lib.cfg.actionShortcuts["switch-window"];
    CHECK(combos.size() == 1);
    CHECK(combos.size() == 1 && combos[0].mods == ModAlt && combos[0].key == 'Q');
    CHECK(lib.cfg.actionShortcuts.count("copy") == 0);  // no shortcut declared
}

void severalDefaultShortcutsAndBadOnes() {
    auto two = parse("[action a]\nshortcut: alt+q\nshortcut: alt+w\nalt+tab\n");
    CHECK(two.ok);
    CHECK(two.cfg.actionShortcuts["a"].size() == 2);
    CHECK(!parse("[action a]\nshortcut: alt+nokey\nalt+tab\n").ok);  // unknown key
    CHECK(!parse("[action a]\nshortcut: foo+q\nalt+tab\n").ok);      // unknown modifier
}

void defaultShortcutIsBoundWithoutAnyUserConfig() {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("", lib.cfg);
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr);
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModAlt && b->out[0].key == VK_TAB);
}

void noLibraryMeansNoDefaults() {
    auto p = parse("");
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.empty());
}

void userBindingReplacesTheDefault() {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+q = @copy\n", lib.cfg);
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1);  // one binding for alt+q, the user's
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModCtrl && b->out[0].key == 'C');
}

void defaultShortcutCanBeCleared() {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+q = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(p.errors.empty());
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') == nullptr);
    CHECK(p.cfg.base.chords.empty());
}

void clearingOneDefaultKeepsTheOthers() {
    auto lib = parse("[action a]\nshortcut: alt+q\nalt+tab\n[action b]\nshortcut: alt+w\nctrl+c\n");
    auto p = parseWithLibrary("alt+q = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') == nullptr);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'W') != nullptr);
}

void clearedShortcutCanBeRebound() {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+q = none\nalt+q = @copy\n", lib.cfg);  // last line wins
    CHECK(p.ok);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].key == 'C');
}

void clearingAnUnboundComboIsHarmless() {
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("alt+z = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') != nullptr);  // default untouched
}

void clearWorksWithoutALibraryToo() {
    auto p = parse("alt+q = @switch\n[action switch]\nalt+tab\n");
    CHECK(p.ok && p.cfg.base.chords.size() == 1);
    auto q = parse("alt+q = @switch\nalt+q = none\n[action switch]\nalt+tab\n");
    CHECK(q.ok);
    CHECK(q.cfg.base.chords.empty());  // a user binding is cleared the same way
}

void clearIsPerScope() {
    // Clearing alt+q in a layer doesn't touch the base default.
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("[layer CapsLock]\nalt+q = none\n", lib.cfg);
    CHECK(p.ok);
    CHECK(findBinding(p.cfg.base.chords, ModAlt, 'Q') != nullptr);
}

void userRedefinedActionKeepsItsDefaultShortcut() {
    // The user redefines switch-window; the default combo now runs the user's steps.
    auto lib = parse(kLibWithDefault);
    auto p = parseWithLibrary("[action switch-window]\nctrl+tab\n", lib.cfg);
    CHECK(p.ok);
    const Binding* b = findBinding(p.cfg.base.chords, ModAlt, 'Q');
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModCtrl);
}

// ---- media keys ----

void mediaKeyNames() {
    CHECK(vkFromName("volumeup").value_or(0) == VK_VOLUME_UP);
    CHECK(vkFromName("volumedown").value_or(0) == VK_VOLUME_DOWN);
    CHECK(vkFromName("mute").value_or(0) == VK_VOLUME_MUTE);
    CHECK(vkFromName("playpause").value_or(0) == VK_MEDIA_PLAY_PAUSE);
    CHECK(vkFromName("nexttrack").value_or(0) == VK_MEDIA_NEXT_TRACK);
    CHECK(vkFromName("previoustrack").value_or(0) == VK_MEDIA_PREV_TRACK);
    CHECK(vkFromName("browserback").value_or(0) == VK_BROWSER_BACK);
    CHECK(nameFromVk(VK_VOLUME_UP) == "volumeup");  // reverse lookup works for them too
}

void mediaKeyActionIsBindable() {
    auto lib = parse("[action volume-up]\ndescription: Raise the volume\nvolumeup\n");
    CHECK(lib.ok);
    CHECK(lib.cfg.actions["volume-up"].size() == 1 && lib.cfg.actions["volume-up"][0].key == VK_VOLUME_UP);
    CHECK(lib.cfg.actionShortcuts.count("volume-up") == 0);  // no default combo

    Config cfg;
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+up = @volume-up\n", cfg, errors, &lib.cfg));
    const Binding* b = findBinding(cfg.base.chords, ModAlt, VK_UP);
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == 0 && b->out[0].key == VK_VOLUME_UP);
    CHECK(cfg.base.chords.size() == 1);  // and nothing else got bound by default
}

void mediaKeyWorksInComboOutput() {
    auto p = parse("alt+down = volumedown\n");  // a media key as the plain target of a combo
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1 && p.cfg.base.chords[0].out[0].key == VK_VOLUME_DOWN);
}

}  // namespace

int main() {
    altQRunsSwitchWindowAction();
    altQBindingMatchesOnlyExactChord();
    altQSendsTabWhileAltStaysHeld();
    expandPressesMissingModifiers();
    expandSwapsOutPhysicalModifiers();
    expandRunsStepsInOrder();
    commaSeparatesSteps();
    unknownActionIsAnError();
    actionMayBeDefinedAfterItsBinding();
    plainKeyMappingsStayInTheKeyMap();
    chordBindingsWorkInsideALayer();
    comboOutputWithoutAction();
    badChordIsAnError();
    modifierKeyDetection();
    descriptionIsParsed();
    libraryActionIsUsedWhenNotDefined();
    userActionOverridesLibrary();
    keyNameFromVk();
    defaultShortcutIsParsedInTheLibrary();
    severalDefaultShortcutsAndBadOnes();
    defaultShortcutIsBoundWithoutAnyUserConfig();
    noLibraryMeansNoDefaults();
    userBindingReplacesTheDefault();
    defaultShortcutCanBeCleared();
    clearingOneDefaultKeepsTheOthers();
    clearedShortcutCanBeRebound();
    clearingAnUnboundComboIsHarmless();
    clearWorksWithoutALibraryToo();
    clearIsPerScope();
    userRedefinedActionKeepsItsDefaultShortcut();
    mediaKeyNames();
    mediaKeyActionIsBindable();
    mediaKeyWorksInComboOutput();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}

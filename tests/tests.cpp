// Minimal test runner (no external dependencies): run keymapper_tests.exe, exit code 0 = pass.
#include <iostream>
#include <string>
#include <vector>

#include "config_loader.h"
#include "engine.h"
#include "key_names.h"
#include "keycodes.h"
#include "matcher.h"
#include "static_files.h"

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
        if (a[i].key != b[i].key || a[i].up != b[i].up) return false;
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
    CHECK(steps[0].key == key::Tab);

    // The binding lives in the base chords (not the plain key map) and carries the action's steps.
    CHECK(p.cfg.base.map.empty());
    CHECK(p.cfg.base.chords.size() == 1);
    const Binding& b = p.cfg.base.chords[0];
    CHECK(b.from.mods == ModAlt);
    CHECK(b.from.key == 'Q');
    CHECK(b.out.size() == 1 && b.out[0].mods == ModAlt && b.out[0].key == key::Tab);
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
    CHECK(same(expand(b->out, {key::LAlt}), {{key::Tab, false}, {key::Tab, true}}));
    // Right Alt works the same.
    CHECK(same(expand(b->out, {key::RAlt}), {{key::Tab, false}, {key::Tab, true}}));
}

// ---- expansion of other shapes ----

void expandPressesMissingModifiers() {
    Steps s = {{ModAlt, key::Tab}};
    CHECK(same(expand(s, {}), {{key::LAlt, false}, {key::Tab, false}, {key::Tab, true}, {key::LAlt, true}}));
}

void expandSwapsOutPhysicalModifiers() {
    // Output wants Ctrl+C but the user is physically holding Alt: release Alt, send, restore Alt.
    Steps s = {{ModCtrl, 'C'}};
    CHECK(same(expand(s, {key::LAlt}),
               {{key::LAlt, true}, {key::LControl, false}, {'C', false}, {'C', true}, {key::LControl, true}, {key::LAlt, false}}));
}

void expandRunsStepsInOrder() {
    Steps s = {{ModCtrl, 'C'}, {ModCtrl, 'V'}};
    CHECK(same(expand(s, {}),
               {{key::LControl, false}, {'C', false}, {'C', true}, {key::LControl, true},
                {key::LControl, false}, {'V', false}, {'V', true}, {key::LControl, true}}));
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
    CHECK(modBit(key::LAlt) == ModAlt);
    CHECK(modBit(key::RAlt) == ModAlt);
    CHECK(modBit(key::LControl) == ModCtrl);
    CHECK(modBit(key::RShift) == ModShift);
    CHECK(modBit(key::LWin) == ModWin);
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
    CHECK(cfg.base.chords.size() == 1 && cfg.base.chords[0].out.size() == 1 && cfg.base.chords[0].out[0].key == key::Tab);

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
    CHECK(nameFromVk(key::Tab) == "tab");
    CHECK(nameFromVk(key::Escape) == "esc");  // shortest of esc/escape
    CHECK(nameFromVk(key::Enter) == "enter");
    CHECK(nameFromVk(key::Period) == ".");
    CHECK(nameFromVk('Q') == "q");
    CHECK(nameFromVk(key::F4) == "f4");
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
    CHECK(b != nullptr && b->out.size() == 1 && b->out[0].mods == ModAlt && b->out[0].key == key::Tab);
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
    CHECK(vkFromName("volumeup").value_or(0) == key::VolumeUp);
    CHECK(vkFromName("volumedown").value_or(0) == key::VolumeDown);
    CHECK(vkFromName("mute").value_or(0) == key::VolumeMute);
    CHECK(vkFromName("playpause").value_or(0) == key::MediaPlayPause);
    CHECK(vkFromName("nexttrack").value_or(0) == key::MediaNext);
    CHECK(vkFromName("previoustrack").value_or(0) == key::MediaPrevious);
    CHECK(vkFromName("browserback").value_or(0) == key::BrowserBack);
    CHECK(nameFromVk(key::VolumeUp) == "volumeup");  // reverse lookup works for them too
}

void mediaKeyActionIsBindable() {
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

void mediaKeyWorksInComboOutput() {
    auto p = parse("alt+down = volumedown\n");  // a media key as the plain target of a combo
    CHECK(p.ok);
    CHECK(p.cfg.base.chords.size() == 1 && p.cfg.base.chords[0].out[0].key == key::VolumeDown);
}

// ---- action categories ----

void categoryIsParsed() {
    auto p = parse("[action a]\ndescription: A\ncategory: Windows and desktops\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionCategory["a"] == "Windows and desktops");
    CHECK(p.cfg.actions["a"].size() == 1);  // the category line is not a step
}

void categoryIsOptional() {
    auto p = parse("[action a]\nalt+tab\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionCategory.count("a") == 0);
}

void categoryAndOtherLinesCanBeInAnyOrder() {
    auto p = parse("[action a]\nalt+tab\nshortcut: alt+q\ncategory: Windows\ndescription: Switch\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionCategory["a"] == "Windows");
    CHECK(p.cfg.actionInfo["a"] == "Switch");
    CHECK(p.cfg.actionShortcuts["a"].size() == 1);
}

void actionOrderFollowsTheFile() {
    auto p = parse("[action zeta]\nalt+tab\n[action alpha]\nctrl+c\n[action mid]\nctrl+v\n");
    CHECK(p.ok);
    CHECK(p.cfg.actionOrder.size() == 3);
    CHECK(p.cfg.actionOrder.size() == 3 && p.cfg.actionOrder[0] == "zeta" && p.cfg.actionOrder[1] == "alpha" && p.cfg.actionOrder[2] == "mid");
}

// ---- static files served to the GUI ----

void staticPathsAreSafe() {
    CHECK(isSafeStaticPath("/js/app.js"));
    CHECK(isSafeStaticPath("/css/app.css"));
    CHECK(isSafeStaticPath("/js/keyboard-layout.js"));
    CHECK(!isSafeStaticPath(""));
    CHECK(!isSafeStaticPath("/"));
    CHECK(!isSafeStaticPath("js/app.js"));              // must start with '/'
    CHECK(!isSafeStaticPath("/../secret.txt"));
    CHECK(!isSafeStaticPath("/js/../../secret.txt"));
    CHECK(!isSafeStaticPath("/js/..\\..\\secret.txt"));  // backslashes
    CHECK(!isSafeStaticPath("//server/share/x.js"));
    CHECK(!isSafeStaticPath("/c:/windows/win.ini"));    // drive letters
    CHECK(!isSafeStaticPath("/js/%2e%2e/x.js"));        // encoded dots
    CHECK(!isSafeStaticPath("/.git/config"));            // hidden files
    CHECK(!isSafeStaticPath("/js/app.js?x=1"));          // the query is stripped before this check
    CHECK(!isSafeStaticPath(std::string("/js/") + std::string(300, 'a') + ".js"));  // absurdly long
}

void staticContentTypes() {
    CHECK(std::string(contentTypeFor("/js/app.js")).find("text/javascript") == 0);
    CHECK(std::string(contentTypeFor("/css/app.css")).find("text/css") == 0);
    CHECK(std::string(contentTypeFor("/index.html")).find("text/html") == 0);
    CHECK(std::string(contentTypeFor("/data/x.json")).find("application/json") == 0);
    CHECK(std::string(contentTypeFor("/img/a.svg")) == "image/svg+xml");
    CHECK(contentTypeFor("/mappings.txt") == nullptr);   // only web assets are served
    CHECK(contentTypeFor("/keymapper.exe") == nullptr);
    CHECK(contentTypeFor("/noextension") == nullptr);
}

// ---- engine: what to do with each physical key event (the platform-independent part of the hook) ----

std::shared_ptr<const Config> cfgOf(const std::string& text) {
    auto cfg = std::make_shared<Config>();
    std::vector<std::string> errors;
    parseConfig(text, *cfg, errors);
    return cfg;
}

struct Out {
    bool swallow;
    std::vector<KeyEvent> send;
};

Out press(Engine& e, const std::string& dev, unsigned short k, bool up = false) {
    Out o{};
    o.swallow = e.handle(dev, k, up, o.send);
    return o;
}

void engineRemapsAKeyAndItsRepeatAndRelease() {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    auto down = press(e, "K1", key::Tab);
    CHECK(down.swallow && same(down.send, {{key::CapsLock, false}}));
    auto repeat = press(e, "K1", key::Tab);  // auto-repeat
    CHECK(repeat.swallow && same(repeat.send, {{key::CapsLock, false}}));
    auto up = press(e, "K1", key::Tab, true);
    CHECK(up.swallow && same(up.send, {{key::CapsLock, true}}));
}

void engineLeavesOtherKeysAlone() {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    auto a = press(e, "K1", 'A');
    CHECK(!a.swallow && a.send.empty());
    auto aUp = press(e, "K1", 'A', true);
    CHECK(!aUp.swallow && aUp.send.empty());
}

void engineLayerKeyIsSwallowedAndRemapsWhileHeld() {
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

void engineReleasesWhatItPressedEvenAfterTheLayerIsGone() {
    Engine e;
    e.setConfig("", cfgOf("[layer CapsLock]\nh = left\n"));
    press(e, "K1", key::CapsLock);
    press(e, "K1", 'H');
    press(e, "K1", key::CapsLock, true);          // layer key released first
    auto hUp = press(e, "K1", 'H', true);          // h must still release Left, not stay stuck
    CHECK(hUp.swallow && same(hUp.send, {{key::Left, true}}));
}

void engineChordRunsActionWhileModifierHeld() {
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

void engineChordNeedsExactlyThoseModifiers() {
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

void engineChordSwapsPhysicalModifiers() {
    Engine e;
    e.setConfig("", cfgOf("alt+w = ctrl+c\n"));
    press(e, "K1", key::LAlt);
    auto w = press(e, "K1", 'W');
    CHECK(w.swallow);
    CHECK(same(w.send, {{key::LAlt, true}, {key::LControl, false}, {'C', false}, {'C', true}, {key::LControl, true}, {key::LAlt, false}}));
}

void engineKeepsConfigPerKeyboard() {
    Engine e;
    e.setConfig("", cfgOf(""));
    e.setConfig("A", cfgOf("tab = capslock\n"));
    CHECK(press(e, "A", key::Tab).swallow);
    press(e, "A", key::Tab, true);
    CHECK(!press(e, "B", key::Tab).swallow);       // B has no config of its own: the (empty) default
}

void engineKeepsLayerAndModifierStatePerKeyboard() {
    Engine e;
    e.setConfig("", cfgOf("[layer CapsLock]\nh = left\n[action sw]\nalt+tab\nalt+q = @sw\n"));
    press(e, "A", key::CapsLock);                   // layer held on keyboard A...
    CHECK(!press(e, "B", 'H').swallow);             // ...does not affect keyboard B
    press(e, "A", key::LAlt);                       // Alt held on A...
    CHECK(!press(e, "B", 'Q').swallow);             // ...is not Alt+Q on B
}

void engineReleaseFindsTheKeyEvenIfAttributedToAnotherKeyboard() {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    press(e, "A", key::Tab);
    auto up = press(e, "B", key::Tab, true);        // release arrives attributed to B (a wrong guess)
    CHECK(up.swallow && same(up.send, {{key::CapsLock, true}}));
}

void engineConfigChangeDoesNotLeaveKeysStuck() {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    press(e, "K1", key::Tab);
    e.setConfig("", cfgOf(""));                     // config changes while the key is down
    auto up = press(e, "K1", key::Tab, true);
    CHECK(up.swallow && same(up.send, {{key::CapsLock, true}}));
    CHECK(!press(e, "K1", key::Tab).swallow);       // and the new (empty) config applies from now on
}

void engineNullConfigRemovesAKeyboardsOwn() {
    Engine e;
    e.setConfig("", cfgOf("tab = capslock\n"));
    e.setConfig("A", cfgOf(""));                    // A overrides the default with nothing
    CHECK(!press(e, "A", key::Tab).swallow);
    e.setConfig("A", nullptr);                      // remove A's own config: back to the default
    CHECK(press(e, "A", key::Tab).swallow);
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
    categoryIsParsed();
    categoryIsOptional();
    categoryAndOtherLinesCanBeInAnyOrder();
    actionOrderFollowsTheFile();
    staticPathsAreSafe();
    staticContentTypes();
    engineRemapsAKeyAndItsRepeatAndRelease();
    engineLeavesOtherKeysAlone();
    engineLayerKeyIsSwallowedAndRemapsWhileHeld();
    engineReleasesWhatItPressedEvenAfterTheLayerIsGone();
    engineChordRunsActionWhileModifierHeld();
    engineChordNeedsExactlyThoseModifiers();
    engineChordSwapsPhysicalModifiers();
    engineKeepsConfigPerKeyboard();
    engineKeepsLayerAndModifierStatePerKeyboard();
    engineReleaseFindsTheKeyEvenIfAttributedToAnotherKeyboard();
    engineConfigChangeDoesNotLeaveKeysStuck();
    engineNullConfigRemovesAKeyboardsOwn();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}

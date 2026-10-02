// Minimal test runner (no external dependencies): run keymapper_tests.exe, exit code 0 = pass.
#include <windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "config_loader.h"
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

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;
}

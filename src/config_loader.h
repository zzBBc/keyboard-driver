#pragma once
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// virtual-key (from) -> virtual-key (to)
using KeyMap = std::unordered_map<unsigned short, unsigned short>;

enum Mod : unsigned { ModCtrl = 1, ModAlt = 2, ModShift = 4, ModWin = 8 };

// Modifiers held together with one key, e.g. Alt+Tab = {ModAlt, VK_TAB}.
struct KeyChord {
    unsigned mods = 0;
    unsigned short key = 0;
};
using Steps = std::vector<KeyChord>;  // tapped one after another

// `from` (exact modifiers + key) produces `out`. Empty `out` = unresolved, never fires.
struct Binding {
    KeyChord from;
    Steps out;
};

// Mappings that apply together: plain key -> key, and modifier/action bindings.
struct Scope {
    KeyMap map;
    std::vector<Binding> chords;
};

// While `trigger` is held, the layer's mappings take priority over the base's.
struct Layer : Scope {
    unsigned short trigger = 0;
};

struct Config {
    Scope base;
    std::vector<Layer> layers;                 // earlier layers win when several are held
    std::map<std::string, Steps> actions;      // named sequences, e.g. "switch-window" = Alt+Tab
};

// Parse config text:
//   from = to            (base mapping, before any section header)
//   [layer <key>]        (following mappings apply while <key> is held)
//   [base]               (switch back to base mappings)
//   [action <name>]      (following lines are the action's steps: "ctrl+c, ctrl+v")
// `from` is a key or a chord ("alt+q"); `to` is a key, chord, comma-separated steps, or
// "@action". Problems are appended to `errors` (with line numbers) and the line is skipped.
// Returns true if there were no errors.
bool parseConfig(const std::string& text, Config& out, std::vector<std::string>& errors);

bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& content);

// Hardware ids look like "VID_046D&PID_C52B" (or e.g. "PNP0303" for a PS/2 keyboard).
// Only upper-case letters, digits, '_' and '&' are valid, so they are safe as file names.
bool validDeviceId(const std::string& id);

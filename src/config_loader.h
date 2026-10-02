#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// virtual-key (from) -> virtual-key (to)
using KeyMap = std::unordered_map<unsigned short, unsigned short>;

// While `trigger` is held, `map` takes priority over the base map.
struct Layer {
    unsigned short trigger;
    KeyMap map;
};

struct Config {
    KeyMap base;
    std::vector<Layer> layers;  // earlier layers win when several are held
};

// Parse config text:
//   from = to            (base mapping, before any section header)
//   [layer <key>]        (following mappings apply while <key> is held)
//   [base]               (switch back to base mappings)
// Problems are appended to `errors` (with line numbers) and the line is skipped.
// Returns true if there were no errors.
bool parseConfig(const std::string& text, Config& out, std::vector<std::string>& errors);

bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& content);

// Hardware ids look like "VID_046D&PID_C52B" (or e.g. "PNP0303" for a PS/2 keyboard).
// Only upper-case letters, digits, '_' and '&' are valid, so they are safe as file names.
bool validDeviceId(const std::string& id);

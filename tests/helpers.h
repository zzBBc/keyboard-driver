#pragma once
// Helpers shared by the test files.
#include <memory>
#include <string>
#include <vector>

#include "config_loader.h"
#include "engine.h"
#include "matcher.h"

struct Parsed {
    Config cfg;
    std::vector<std::string> errors;
    bool ok;
};

inline Parsed parse(const std::string& text) {
    Parsed p;
    p.ok = parseConfig(text, p.cfg, p.errors);
    return p;
}

inline bool same(const std::vector<KeyEvent>& a, const std::vector<KeyEvent>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].key != b[i].key || a[i].up != b[i].up) return false;
    return true;
}

inline constexpr const char* kAltQ =
    "[action switch-window]\n"
    "alt+tab\n"
    "\n"
    "alt+q = @switch-window\n";

inline constexpr const char* kLibWithDefault =
    "[action switch-window]\n"
    "description: Switch window\n"
    "shortcut: alt+q\n"
    "alt+tab\n"
    "\n"
    "[action copy]\n"
    "ctrl+c\n";

inline Parsed parseWithLibrary(const std::string& text, const Config& lib) {
    Parsed p;
    p.ok = parseConfig(text, p.cfg, p.errors, &lib);
    return p;
}

inline std::shared_ptr<const Config> cfgOf(const std::string& text) {
    auto cfg = std::make_shared<Config>();
    std::vector<std::string> errors;
    parseConfig(text, *cfg, errors);
    return cfg;
}

struct Out {
    bool swallow;
    std::vector<KeyEvent> send;
};

inline Out press(Engine& e, const std::string& dev, unsigned short k, bool up = false) {
    Out o{};
    o.swallow = e.handle(dev, k, up, o.send);
    return o;
}

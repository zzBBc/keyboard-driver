#include "config_loader.h"

#include <fstream>
#include <sstream>

#include "key_names.h"

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// "alt+shift+tab" -> {ModAlt|ModShift, VK_TAB}. On failure returns false and sets `err`.
bool parseChord(const std::string& text, KeyChord& out, std::string& err) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (;;) {
        size_t plus = text.find('+', start);
        parts.push_back(trim(text.substr(start, plus == std::string::npos ? std::string::npos : plus - start)));
        if (plus == std::string::npos) break;
        start = plus + 1;
    }

    KeyChord chord;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::string m = lower(parts[i]);
        if (m == "ctrl") chord.mods |= ModCtrl;
        else if (m == "alt") chord.mods |= ModAlt;
        else if (m == "shift") chord.mods |= ModShift;
        else if (m == "win") chord.mods |= ModWin;
        else { err = "unknown modifier '" + parts[i] + "' (use ctrl, alt, shift, win)"; return false; }
    }
    auto vk = vkFromName(parts.back());
    if (!vk) { err = "unknown key '" + parts.back() + "'"; return false; }
    chord.key = *vk;
    out = chord;
    return true;
}

// "ctrl+c, ctrl+v" -> two steps. The whole text is tried as one chord first so "," works as a key.
bool parseSteps(const std::string& text, Steps& out, std::string& err) {
    KeyChord single;
    std::string singleErr;
    if (parseChord(text, single, singleErr)) { out = {single}; return true; }

    Steps steps;
    size_t start = 0;
    for (;;) {
        size_t comma = text.find(',', start);
        KeyChord c;
        if (!parseChord(text.substr(start, comma == std::string::npos ? std::string::npos : comma - start), c, err))
            return false;
        steps.push_back(c);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    out = std::move(steps);
    return true;
}

bool validActionName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}

// A binding waiting for its action to be defined (actions may come after the bindings using them).
struct Pending {
    int lineNo;
    std::string action;
    int layer;  // -1 = base
    size_t index;  // into that scope's chords
};

}  // namespace

bool parseConfig(const std::string& text, Config& out, std::vector<std::string>& errors) {
    out = Config{};
    const size_t errorsBefore = errors.size();
    auto fail = [&](int lineNo, const std::string& msg) {
        errors.push_back("line " + std::to_string(lineNo) + ": " + msg);
    };

    enum class Mode { Mappings, Action, Skip };
    Mode mode = Mode::Mappings;
    Scope* scope = &out.base;
    int scopeLayer = -1;
    Steps* action = nullptr;
    std::vector<Pending> pending;

    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']') { fail(lineNo, "missing ']'"); mode = Mode::Skip; continue; }
            std::string inner = trim(line.substr(1, line.size() - 2));
            const std::string low = lower(inner);
            if (low == "base") { mode = Mode::Mappings; scope = &out.base; scopeLayer = -1; continue; }
            if (low.rfind("layer", 0) == 0) {
                std::string keyName = trim(inner.substr(5));
                auto vk = vkFromName(keyName);
                if (!vk) { fail(lineNo, "unknown layer key '" + keyName + "'"); mode = Mode::Skip; continue; }
                out.layers.emplace_back();
                out.layers.back().trigger = *vk;
                mode = Mode::Mappings;
                scope = &out.layers.back();
                scopeLayer = static_cast<int>(out.layers.size()) - 1;
                continue;
            }
            if (low.rfind("action", 0) == 0) {
                std::string name = lower(trim(inner.substr(6)));
                if (!validActionName(name)) {
                    fail(lineNo, "invalid action name '" + trim(inner.substr(6)) + "' (use letters, digits, - and _)");
                    mode = Mode::Skip;
                } else if (out.actions.count(name)) {
                    fail(lineNo, "action '" + name + "' is already defined");
                    mode = Mode::Skip;
                } else {
                    mode = Mode::Action;
                    action = &out.actions[name];
                }
                continue;
            }
            fail(lineNo, "unknown section '" + inner + "'");
            mode = Mode::Skip;
            continue;
        }

        if (mode == Mode::Skip) continue;  // bad section, already reported

        // Position of the '=' in "from = to", or npos. Skips a '=' that follows '+' ("alt+=" is a chord).
        auto findEq = [](const std::string& l) {
            auto eq = l.find('=', 1);  // start at 1 so "=" can be a key name
            while (eq != std::string::npos && l[eq - 1] == '+') eq = l.find('=', eq + 1);
            return eq;
        };

        if (mode == Mode::Action) {
            Steps steps;
            std::string err;
            if (parseSteps(line, steps, err)) { action->insert(action->end(), steps.begin(), steps.end()); continue; }
            if (findEq(line) == std::string::npos) { fail(lineNo, err); continue; }
            // A "from = to" line ends the action: back to base mappings.
            mode = Mode::Mappings;
            scope = &out.base;
            scopeLayer = -1;
        }

        const auto eq = findEq(line);
        if (eq == std::string::npos) { fail(lineNo, "expected 'from = to'"); continue; }
        const std::string from = trim(line.substr(0, eq));
        const std::string to = trim(line.substr(eq + 1));

        KeyChord fromChord;
        std::string err;
        if (!parseChord(from, fromChord, err)) { fail(lineNo, err); continue; }

        Binding binding;
        binding.from = fromChord;
        std::string actionName;
        if (!to.empty() && to.front() == '@') {
            actionName = lower(trim(to.substr(1)));
            if (!validActionName(actionName)) { fail(lineNo, "invalid action name '" + to.substr(1) + "'"); continue; }
        } else if (!parseSteps(to, binding.out, err)) {
            fail(lineNo, err);
            continue;
        }

        const bool plain = actionName.empty() && fromChord.mods == 0 && binding.out.size() == 1 && binding.out[0].mods == 0;
        if (plain) {
            scope->map[fromChord.key] = binding.out[0].key;
            continue;
        }
        // Rebinding the same chord replaces the earlier binding.
        auto& list = scope->chords;
        size_t idx = list.size();
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].from.mods == fromChord.mods && list[i].from.key == fromChord.key) idx = i;
        if (idx == list.size()) list.push_back(binding); else list[idx] = binding;
        if (!actionName.empty()) pending.push_back({lineNo, actionName, scopeLayer, idx});
    }

    for (const auto& p : pending) {
        auto it = out.actions.find(p.action);
        if (it == out.actions.end()) { fail(p.lineNo, "unknown action '" + p.action + "'"); continue; }
        if (it->second.empty()) { fail(p.lineNo, "action '" + p.action + "' has no steps"); continue; }
        Scope& s = p.layer < 0 ? out.base : out.layers[p.layer];
        s.chords[p.index].out = it->second;
    }
    return errors.size() == errorsBefore;
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

bool validDeviceId(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    for (char c : id)
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '&')) return false;
    return true;
}

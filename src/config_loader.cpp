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

}  // namespace

bool parseConfig(const std::string& text, Config& out, std::vector<std::string>& errors) {
    out = Config{};
    const size_t errorsBefore = errors.size();
    auto fail = [&](int lineNo, const std::string& msg) {
        errors.push_back("line " + std::to_string(lineNo) + ": " + msg);
    };

    KeyMap* current = &out.base;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;

        if (line.front() == '[') {
            if (line.back() != ']') { fail(lineNo, "missing ']'"); current = nullptr; continue; }
            std::string inner = trim(line.substr(1, line.size() - 2));
            if (lower(inner) == "base") { current = &out.base; continue; }
            if (lower(inner).rfind("layer", 0) == 0) {
                std::string keyName = trim(inner.substr(5));
                auto vk = vkFromName(keyName);
                if (!vk) { fail(lineNo, "unknown layer key '" + keyName + "'"); current = nullptr; continue; }
                out.layers.push_back(Layer{*vk, {}});
                current = &out.layers.back().map;
                continue;
            }
            fail(lineNo, "unknown section '" + inner + "'");
            current = nullptr;
            continue;
        }

        auto eq = line.find('=', 1);  // start at 1 so "=" can be a key name
        if (eq == std::string::npos) { fail(lineNo, "expected 'from = to'"); continue; }
        std::string from = trim(line.substr(0, eq));
        std::string to = trim(line.substr(eq + 1));

        auto fromVk = vkFromName(from);
        auto toVk = vkFromName(to);
        if (!fromVk || !toVk) { fail(lineNo, "unknown key '" + (fromVk ? to : from) + "'"); continue; }
        if (current) (*current)[*fromVk] = *toVk;  // current == nullptr: bad section, already reported
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

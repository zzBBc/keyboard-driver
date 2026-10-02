#include "key_names.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, unsigned short>& table() {
    static const std::unordered_map<std::string, unsigned short> t = [] {
        std::unordered_map<std::string, unsigned short> m = {
            {"backspace", VK_BACK},   {"tab", VK_TAB},          {"enter", VK_RETURN},
            {"esc", VK_ESCAPE},       {"escape", VK_ESCAPE},    {"space", VK_SPACE},
            {"capslock", VK_CAPITAL}, {"lshift", VK_LSHIFT},    {"rshift", VK_RSHIFT},
            {"lctrl", VK_LCONTROL},   {"rctrl", VK_RCONTROL},   {"lalt", VK_LMENU},
            {"ralt", VK_RMENU},       {"lwin", VK_LWIN},        {"rwin", VK_RWIN},
            {"apps", VK_APPS},        {"pageup", VK_PRIOR},     {"pagedown", VK_NEXT},
            {"home", VK_HOME},        {"end", VK_END},          {"insert", VK_INSERT},
            {"delete", VK_DELETE},    {"left", VK_LEFT},        {"right", VK_RIGHT},
            {"up", VK_UP},            {"down", VK_DOWN},        {"printscreen", VK_SNAPSHOT},
            {"scrolllock", VK_SCROLL},{"pause", VK_PAUSE},      {"numlock", VK_NUMLOCK},
            {";", VK_OEM_1},          {"=", VK_OEM_PLUS},       {",", VK_OEM_COMMA},
            {"-", VK_OEM_MINUS},      {".", VK_OEM_PERIOD},     {"/", VK_OEM_2},
            {"`", VK_OEM_3},          {"[", VK_OEM_4},          {"\\", VK_OEM_5},
            {"]", VK_OEM_6},          {"'", VK_OEM_7},
        };
        for (char c = 'a'; c <= 'z'; ++c) m[std::string(1, c)] = static_cast<unsigned short>(c - 'a' + 'A');
        for (char c = '0'; c <= '9'; ++c) m[std::string(1, c)] = static_cast<unsigned short>(c);
        for (int i = 1; i <= 24; ++i) m["f" + std::to_string(i)] = static_cast<unsigned short>(VK_F1 + i - 1);
        return m;
    }();
    return t;
}

}  // namespace

std::vector<std::string> allKeyNames() {
    std::vector<std::string> names;
    for (const auto& kv : table()) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    return names;
}

std::string nameFromVk(unsigned short vk) {
    std::string best;
    for (const auto& kv : table()) {
        if (kv.second != vk) continue;
        if (best.empty() || kv.first.size() < best.size() || (kv.first.size() == best.size() && kv.first < best)) best = kv.first;
    }
    return best;
}

std::optional<unsigned short> vkFromName(const std::string& name) {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = table().find(key);
    if (it == table().end()) return std::nullopt;
    return it->second;
}

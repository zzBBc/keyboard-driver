#include "key_names.h"

#include "keycodes.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace {

const std::unordered_map<std::string, unsigned short>& table() {
    static const std::unordered_map<std::string, unsigned short> t = [] {
        std::unordered_map<std::string, unsigned short> m = {
            {"backspace", key::Backspace},   {"tab", key::Tab},          {"enter", key::Enter},
            {"esc", key::Escape},       {"escape", key::Escape},    {"space", key::Space},
            {"capslock", key::CapsLock}, {"lshift", key::LShift},    {"rshift", key::RShift},
            {"lctrl", key::LControl},   {"rctrl", key::RControl},   {"lalt", key::LAlt},
            {"ralt", key::RAlt},       {"lwin", key::LWin},        {"rwin", key::RWin},
            {"apps", key::Apps},        {"pageup", key::PageUp},     {"pagedown", key::PageDown},
            {"home", key::Home},        {"end", key::End},          {"insert", key::Insert},
            {"delete", key::Delete},    {"left", key::Left},        {"right", key::Right},
            {"up", key::Up},            {"down", key::Down},        {"printscreen", key::PrintScreen},
            {"scrolllock", key::ScrollLock},{"pause", key::Pause},      {"numlock", key::NumLock},
            {";", key::Semicolon},          {"=", key::Equals},       {",", key::Comma},
            {"-", key::Minus},      {".", key::Period},     {"/", key::Slash},
            {"`", key::Backquote},          {"[", key::LeftBracket},          {"\\", key::Backslash},
            {"]", key::RightBracket},          {"'", key::Quote},
            // Media and browser keys: what many keyboards' Fn combos produce.
            {"volumeup", key::VolumeUp},           {"volumedown", key::VolumeDown},
            {"mute", key::VolumeMute},             {"playpause", key::MediaPlayPause},
            {"nexttrack", key::MediaNext},   {"previoustrack", key::MediaPrevious},
            {"mediastop", key::MediaStop},         {"browserback", key::BrowserBack},
            {"browserforward", key::BrowserForward}, {"browserrefresh", key::BrowserRefresh},
            {"browserhome", key::BrowserHome},     {"launchmail", key::LaunchMail},
        };
        for (char c = 'a'; c <= 'z'; ++c) m[std::string(1, c)] = static_cast<unsigned short>(c - 'a' + 'A');
        for (char c = '0'; c <= '9'; ++c) m[std::string(1, c)] = static_cast<unsigned short>(c);
        for (int i = 1; i <= 24; ++i) m["f" + std::to_string(i)] = static_cast<unsigned short>(key::F1 + i - 1);
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

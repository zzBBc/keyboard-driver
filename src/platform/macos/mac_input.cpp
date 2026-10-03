#include "mac_input.h"

#include "keycodes.h"

#include <algorithm>
#include <cstdio>

namespace mac {

namespace {

// macOS virtual key codes (kVK_* in Carbon's HIToolbox/Events.h), written out so this file needs no
// Apple headers.
enum : uint16_t {
    kVK_ANSI_A = 0x00, kVK_ANSI_S = 0x01, kVK_ANSI_D = 0x02, kVK_ANSI_F = 0x03, kVK_ANSI_H = 0x04,
    kVK_ANSI_G = 0x05, kVK_ANSI_Z = 0x06, kVK_ANSI_X = 0x07, kVK_ANSI_C = 0x08, kVK_ANSI_V = 0x09,
    kVK_ANSI_B = 0x0B, kVK_ANSI_Q = 0x0C, kVK_ANSI_W = 0x0D, kVK_ANSI_E = 0x0E, kVK_ANSI_R = 0x0F,
    kVK_ANSI_Y = 0x10, kVK_ANSI_T = 0x11, kVK_ANSI_1 = 0x12, kVK_ANSI_2 = 0x13, kVK_ANSI_3 = 0x14,
    kVK_ANSI_4 = 0x15, kVK_ANSI_6 = 0x16, kVK_ANSI_5 = 0x17, kVK_ANSI_Equal = 0x18, kVK_ANSI_9 = 0x19,
    kVK_ANSI_7 = 0x1A, kVK_ANSI_Minus = 0x1B, kVK_ANSI_8 = 0x1C, kVK_ANSI_0 = 0x1D,
    kVK_ANSI_RightBracket = 0x1E, kVK_ANSI_O = 0x1F, kVK_ANSI_U = 0x20, kVK_ANSI_LeftBracket = 0x21,
    kVK_ANSI_I = 0x22, kVK_ANSI_P = 0x23, kVK_Return = 0x24, kVK_ANSI_L = 0x25, kVK_ANSI_J = 0x26,
    kVK_ANSI_Quote = 0x27, kVK_ANSI_K = 0x28, kVK_ANSI_Semicolon = 0x29, kVK_ANSI_Backslash = 0x2A,
    kVK_ANSI_Comma = 0x2B, kVK_ANSI_Slash = 0x2C, kVK_ANSI_N = 0x2D, kVK_ANSI_M = 0x2E,
    kVK_ANSI_Period = 0x2F, kVK_Tab = 0x30, kVK_Space = 0x31, kVK_ANSI_Grave = 0x32, kVK_Delete = 0x33,
    kVK_Escape = 0x35, kVK_RightCommand = 0x36, kVK_Command = 0x37, kVK_Shift = 0x38,
    kVK_CapsLock = 0x39, kVK_Option = 0x3A, kVK_Control = 0x3B, kVK_RightShift = 0x3C,
    kVK_RightOption = 0x3D, kVK_RightControl = 0x3E, kVK_Function = 0x3F, kVK_F17 = 0x40,
    kVK_ANSI_KeypadClear = 0x47, kVK_VolumeUp = 0x48, kVK_VolumeDown = 0x49, kVK_Mute = 0x4A,
    kVK_F18 = 0x4F, kVK_F19 = 0x50, kVK_F20 = 0x5A, kVK_F5 = 0x60, kVK_F6 = 0x61, kVK_F7 = 0x62,
    kVK_F3 = 0x63, kVK_F8 = 0x64, kVK_F9 = 0x65, kVK_F11 = 0x67, kVK_F13 = 0x69, kVK_F16 = 0x6A,
    kVK_F14 = 0x6B, kVK_F10 = 0x6D, kVK_ContextualMenu = 0x6E, kVK_F12 = 0x6F, kVK_F15 = 0x71,
    kVK_Help = 0x72, kVK_Home = 0x73, kVK_PageUp = 0x74, kVK_ForwardDelete = 0x75, kVK_F4 = 0x76,
    kVK_End = 0x77, kVK_F2 = 0x78, kVK_PageDown = 0x79, kVK_F1 = 0x7A, kVK_LeftArrow = 0x7B,
    kVK_RightArrow = 0x7C, kVK_DownArrow = 0x7D, kVK_UpArrow = 0x7E,
};

// One row per key: portable key id, macOS key code, HID keyboard usage (0: none). Sided modifiers come
// before the either-side ids so a key code looks up to the sided one.
struct KeyRow {
    unsigned short key;
    uint16_t mac;
    uint32_t hid;
};
constexpr KeyRow kKeys[] = {
    {'A', kVK_ANSI_A, 0x04}, {'B', kVK_ANSI_B, 0x05}, {'C', kVK_ANSI_C, 0x06}, {'D', kVK_ANSI_D, 0x07},
    {'E', kVK_ANSI_E, 0x08}, {'F', kVK_ANSI_F, 0x09}, {'G', kVK_ANSI_G, 0x0A}, {'H', kVK_ANSI_H, 0x0B},
    {'I', kVK_ANSI_I, 0x0C}, {'J', kVK_ANSI_J, 0x0D}, {'K', kVK_ANSI_K, 0x0E}, {'L', kVK_ANSI_L, 0x0F},
    {'M', kVK_ANSI_M, 0x10}, {'N', kVK_ANSI_N, 0x11}, {'O', kVK_ANSI_O, 0x12}, {'P', kVK_ANSI_P, 0x13},
    {'Q', kVK_ANSI_Q, 0x14}, {'R', kVK_ANSI_R, 0x15}, {'S', kVK_ANSI_S, 0x16}, {'T', kVK_ANSI_T, 0x17},
    {'U', kVK_ANSI_U, 0x18}, {'V', kVK_ANSI_V, 0x19}, {'W', kVK_ANSI_W, 0x1A}, {'X', kVK_ANSI_X, 0x1B},
    {'Y', kVK_ANSI_Y, 0x1C}, {'Z', kVK_ANSI_Z, 0x1D},
    {'1', kVK_ANSI_1, 0x1E}, {'2', kVK_ANSI_2, 0x1F}, {'3', kVK_ANSI_3, 0x20}, {'4', kVK_ANSI_4, 0x21},
    {'5', kVK_ANSI_5, 0x22}, {'6', kVK_ANSI_6, 0x23}, {'7', kVK_ANSI_7, 0x24}, {'8', kVK_ANSI_8, 0x25},
    {'9', kVK_ANSI_9, 0x26}, {'0', kVK_ANSI_0, 0x27},
    {key::Enter, kVK_Return, 0x28},
    {key::Escape, kVK_Escape, 0x29},
    {key::Backspace, kVK_Delete, 0x2A},
    {key::Tab, kVK_Tab, 0x2B},
    {key::Space, kVK_Space, 0x2C},
    {key::Minus, kVK_ANSI_Minus, 0x2D},
    {key::Equals, kVK_ANSI_Equal, 0x2E},
    {key::LeftBracket, kVK_ANSI_LeftBracket, 0x2F},
    {key::RightBracket, kVK_ANSI_RightBracket, 0x30},
    {key::Backslash, kVK_ANSI_Backslash, 0x31},
    {key::Semicolon, kVK_ANSI_Semicolon, 0x33},
    {key::Quote, kVK_ANSI_Quote, 0x34},
    {key::Backquote, kVK_ANSI_Grave, 0x35},
    {key::Comma, kVK_ANSI_Comma, 0x36},
    {key::Period, kVK_ANSI_Period, 0x37},
    {key::Slash, kVK_ANSI_Slash, 0x38},
    {key::F1, kVK_F1, 0x3A}, {key::F1 + 1, kVK_F2, 0x3B}, {key::F1 + 2, kVK_F3, 0x3C},
    {key::F1 + 3, kVK_F4, 0x3D}, {key::F1 + 4, kVK_F5, 0x3E}, {key::F1 + 5, kVK_F6, 0x3F},
    {key::F1 + 6, kVK_F7, 0x40}, {key::F1 + 7, kVK_F8, 0x41}, {key::F1 + 8, kVK_F9, 0x42},
    {key::F1 + 9, kVK_F10, 0x43}, {key::F1 + 10, kVK_F11, 0x44}, {key::F1 + 11, kVK_F12, 0x45},
    {key::F1 + 12, kVK_F13, 0x68}, {key::F1 + 13, kVK_F14, 0x69}, {key::F1 + 14, kVK_F15, 0x6A},
    {key::F1 + 15, kVK_F16, 0x6B}, {key::F1 + 16, kVK_F17, 0x6C}, {key::F1 + 17, kVK_F18, 0x6D},
    {key::F1 + 18, kVK_F19, 0x6E}, {key::F1 + 19, kVK_F20, 0x6F},
    {key::Insert, kVK_Help, 0x49},  // a PC keyboard's Insert arrives as Help
    {key::Home, kVK_Home, 0x4A},
    {key::PageUp, kVK_PageUp, 0x4B},
    {key::Delete, kVK_ForwardDelete, 0x4C},
    {key::End, kVK_End, 0x4D},
    {key::PageDown, kVK_PageDown, 0x4E},
    {key::Right, kVK_RightArrow, 0x4F},
    {key::Left, kVK_LeftArrow, 0x50},
    {key::Down, kVK_DownArrow, 0x51},
    {key::Up, kVK_UpArrow, 0x52},
    {key::NumLock, kVK_ANSI_KeypadClear, 0x53},
    {key::Apps, kVK_ContextualMenu, 0x65},
    {key::VolumeMute, kVK_Mute, 0x7F},
    {key::VolumeUp, kVK_VolumeUp, 0x80},
    {key::VolumeDown, kVK_VolumeDown, 0x81},
    {key::LControl, kVK_Control, 0xE0},
    {key::LShift, kVK_Shift, 0xE1},
    {key::LAlt, kVK_Option, 0xE2},
    {key::LWin, kVK_Command, 0xE3},
    {key::RControl, kVK_RightControl, 0xE4},
    {key::RShift, kVK_RightShift, 0xE5},
    {key::RAlt, kVK_RightOption, 0xE6},
    {key::RWin, kVK_RightCommand, 0xE7},
    {key::Ctrl, kVK_Control, 0},
    {key::Shift, kVK_Shift, 0},
    {key::Alt, kVK_Option, 0},
};

// The first row for a key is the one sent; FAST and REWIND are only read.
struct MediaRow {
    unsigned short key;
    int nx;
};
constexpr MediaRow kMedia[] = {
    {key::VolumeUp, 0},         // NX_KEYTYPE_SOUND_UP
    {key::VolumeDown, 1},       // NX_KEYTYPE_SOUND_DOWN
    {key::VolumeMute, 7},       // NX_KEYTYPE_MUTE
    {key::MediaPlayPause, 16},  // NX_KEYTYPE_PLAY
    {key::MediaNext, 17},       // NX_KEYTYPE_NEXT
    {key::MediaPrevious, 18},   // NX_KEYTYPE_PREVIOUS
    {key::MediaNext, 19},       // NX_KEYTYPE_FAST
    {key::MediaPrevious, 20},   // NX_KEYTYPE_REWIND
};

constexpr int kMediaDown = 0xA, kMediaUp = 0xB;

constexpr uint32_t kHidCapsLock = 0x39;
constexpr uint64_t kHidKeyboardPage = 0x700000000;

// HID usage of a key; an either-side modifier (Ctrl...) uses its left key's.
std::optional<uint32_t> hidFromKey(unsigned short key) {
    if (key == key::CapsLock) return kHidCapsLock;
    const auto code = macFromKey(key);
    if (!code) return std::nullopt;
    for (const auto& r : kKeys)
        if (r.mac == *code && r.hid) return r.hid;
    return std::nullopt;
}

void setHeld(std::set<uint16_t>& mods, uint16_t code, bool held) {
    if (held) mods.insert(code);
    else mods.erase(code);
}

}  // namespace

std::optional<unsigned short> keyFromMac(uint16_t code) {
    for (const auto& r : kKeys)
        if (r.mac == code) return r.key;
    return std::nullopt;
}

std::optional<uint16_t> macFromKey(unsigned short key) {
    if (key == key::Fn) return kVK_Function;  // send only: a physical Fn is never reported (see keyFromMac)
    for (const auto& r : kKeys)
        if (r.key == key) return r.mac;
    return std::nullopt;
}

std::optional<unsigned short> keyFromHid(uint32_t usage) {
    for (const auto& r : kKeys)
        if (r.hid && r.hid == usage) return r.key;
    return std::nullopt;
}

Flags modifierFlags(uint16_t code) {
    switch (code) {
        case kVK_Control: return kFlagControl | 0x00000001;
        case kVK_RightControl: return kFlagControl | 0x00002000;
        case kVK_Shift: return kFlagShift | 0x00000002;
        case kVK_RightShift: return kFlagShift | 0x00000004;
        case kVK_Command: return kFlagCommand | 0x00000008;
        case kVK_RightCommand: return kFlagCommand | 0x00000010;
        case kVK_Option: return kFlagAlternate | 0x00000020;
        case kVK_RightOption: return kFlagAlternate | 0x00000040;
        case kVK_Function: return kFlagSecondaryFn;  // no left/right bit: there is one Fn key
        default: return 0;
    }
}

Flags keyFlags(uint16_t code) {
    switch (code) {
        case kVK_LeftArrow: case kVK_RightArrow: case kVK_UpArrow: case kVK_DownArrow:
            return kFlagSecondaryFn | kFlagNumericPad;
        case kVK_Home: case kVK_End: case kVK_PageUp: case kVK_PageDown: case kVK_ForwardDelete: case kVK_Help:
        case kVK_F1: case kVK_F2: case kVK_F3: case kVK_F4: case kVK_F5: case kVK_F6: case kVK_F7: case kVK_F8:
        case kVK_F9: case kVK_F10: case kVK_F11: case kVK_F12: case kVK_F13: case kVK_F14: case kVK_F15:
        case kVK_F16: case kVK_F17: case kVK_F18: case kVK_F19: case kVK_F20:
            return kFlagSecondaryFn;
        default:
            return 0;
    }
}

bool isModifierRelease(uint16_t code, Flags flags) {
    return (flags & modifierFlags(code) & kSideBits) == 0;
}

MediaKey decodeMedia(long data1) {
    return {static_cast<int>((data1 & 0xFFFF0000) >> 16), ((data1 & 0xFF00) >> 8) == kMediaUp};
}

long encodeMedia(int nx, bool up) {
    return (static_cast<long>(nx) << 16) | ((up ? kMediaUp : kMediaDown) << 8);
}

std::optional<unsigned short> keyFromMedia(int nx) {
    for (const auto& m : kMedia)
        if (m.nx == nx) return m.key;
    return std::nullopt;
}

std::optional<int> mediaFromKey(unsigned short key) {
    for (const auto& m : kMedia)
        if (m.key == key) return m.nx;
    return std::nullopt;
}

std::vector<HidMapping> takeCapsLockMappings(KeyMap& map) {
    std::vector<HidMapping> out;
    for (auto it = map.begin(); it != map.end();) {
        const auto src = hidFromKey(it->first), dst = hidFromKey(it->second);
        if ((it->first == key::CapsLock || it->second == key::CapsLock) && src && dst) {
            out.push_back({kHidKeyboardPage | *src, kHidKeyboardPage | *dst});
            it = map.erase(it);
        } else {
            ++it;
        }
    }
    std::sort(out.begin(), out.end(), [](const HidMapping& a, const HidMapping& b) { return a.src < b.src; });
    return out;
}

std::string userKeyMappingJson(const std::vector<HidMapping>& mappings) {
    std::string json = "{\"UserKeyMapping\":[";
    for (size_t i = 0; i < mappings.size(); ++i) {
        if (i) json += ",";
        json += "{\"HIDKeyboardModifierMappingSrc\":" + std::to_string(mappings[i].src) +
                ",\"HIDKeyboardModifierMappingDst\":" + std::to_string(mappings[i].dst) + "}";
    }
    return json + "]}";
}

std::string deviceId(int vendorId, int productId) {
    if (vendorId == 0 && productId == 0) return "UNKNOWN";
    char buf[32];
    std::snprintf(buf, sizeof buf, "VID_%04X&PID_%04X", vendorId & 0xFFFF, productId & 0xFFFF);
    return buf;
}

void ModifierState::physical(uint16_t code, bool up, bool swallowed) {
    if (swallowed) {
        setHeld(swallowed_, code, !up);
        return;
    }
    setHeld(held_, code, !up);
    if (up) {  // a physical release ends any press we injected or swallowed for it
        injected_.erase(code);
        swallowed_.erase(code);
    }
}

void ModifierState::injected(uint16_t code, bool up) {
    setHeld(held_, code, !up);
    setHeld(injected_, code, !up);
}

Flags ModifierState::apply(Flags flags) const {
    Flags mods = 0;
    for (uint16_t code : held_) mods |= modifierFlags(code);
    return (flags & ~kAllModifierFlags) | mods;
}

void Attributor::hid(unsigned short key, bool up, const std::string& device, Clock::time_point now) {
    if (!up) last_ = device;
    pending_.push_back({key, up, device, now});
    if (pending_.size() > kMaxPending) pending_.pop_front();
}

std::string Attributor::attribute(unsigned short key, bool up, Clock::time_point now) {
    while (!pending_.empty() && now - pending_.front().time > kTtl) pending_.pop_front();
    for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->key == key && it->up == up) {
            std::string id = it->device;
            pending_.erase(pending_.begin(), it + 1);  // older entries are stale
            return id;
        }
    }
    return last_;
}

}  // namespace mac

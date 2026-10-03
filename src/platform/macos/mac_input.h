#pragma once
// The macOS layer's decisions, kept apart from the Apple APIs so they can be tested on any OS: key code
// tables, modifier flags, media key events, keyboard ids and matching tap events to keyboards.
// macos_platform.mm feeds these from the event tap and IOHIDManager.
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "config_loader.h"

namespace mac {

// Same values as CGEventFlags (CGEventTypes.h) and the device-dependent bits of IOLLEvent.h.
using Flags = uint64_t;
constexpr Flags kFlagAlphaShift = 0x00010000;  // Caps Lock
constexpr Flags kFlagShift = 0x00020000;
constexpr Flags kFlagControl = 0x00040000;
constexpr Flags kFlagAlternate = 0x00080000;
constexpr Flags kFlagCommand = 0x00100000;
constexpr Flags kFlagNumericPad = 0x00200000;
constexpr Flags kFlagSecondaryFn = 0x00800000;
constexpr Flags kSideBits = 0x207F;  // left/right bit of each modifier
constexpr Flags kAllModifierFlags = kFlagShift | kFlagControl | kFlagAlternate | kFlagCommand | kSideBits;

// macOS virtual key code (kVK_*) <-> portable key id (keycodes.h). Win = Command, Alt = Option.
// Keys macOS has no code for (PrintScreen, ScrollLock, Pause, F21-F24, the browser keys...) map to nothing.
std::optional<unsigned short> keyFromMac(uint16_t code);
std::optional<uint16_t> macFromKey(unsigned short key);

// HID keyboard page usage (what IOHIDManager reports) -> portable key id.
std::optional<unsigned short> keyFromHid(uint32_t usage);

// Generic and left/right flag bits of a modifier key code; 0 for any other key (Caps Lock and Fn too).
Flags modifierFlags(uint16_t code);

// Flags macOS sets on real events for this key (arrows: numeric pad + Fn; F-keys, Home...: Fn).
Flags keyFlags(uint16_t code);

// Whether a flagsChanged event for modifier `code` is its release: its left/right bit is clear.
bool isModifierRelease(uint16_t code, Flags flags);

// Media keys arrive as system-defined events (subtype 8) with an NX_KEYTYPE_* code in data1.
constexpr int kSystemDefined = 14;  // NX_SYSDEFINED
constexpr int kAuxControlButtons = 8;
struct MediaKey {
    int nx;  // NX_KEYTYPE_*
    bool up;
};
MediaKey decodeMedia(long data1);
long encodeMedia(int nx, bool up);
std::optional<unsigned short> keyFromMedia(int nx);
std::optional<int> mediaFromKey(unsigned short key);  // a key sent as a media event, not a key code

// The event tap never sees Caps Lock and can't send it, so plain mappings with Caps Lock on either side
// are done by the HID system instead (hidutil's UserKeyMapping). Values are HID usages (page 7 << 32 | usage).
struct HidMapping {
    uint64_t src;
    uint64_t dst;
    bool operator==(const HidMapping& o) const { return src == o.src && dst == o.dst; }
};
// Moves the Caps Lock mappings out of `map`, sorted by source. A key without a HID usage stays in `map`.
std::vector<HidMapping> takeCapsLockMappings(KeyMap& map);
// The value for `hidutil property --set`.
std::string userKeyMappingJson(const std::vector<HidMapping>& mappings);

// Same id format as on Windows, so a keyboard's config file works on both: "VID_046D&PID_C52B".
std::string deviceId(int vendorId, int productId);

// Which modifiers apps should see as held. A physical modifier we swallow stays held in the system's
// own state, and one we inject is not in it, so while either is true the flags of passed events must
// be rewritten. The rest of the time they are left alone, so other programs' injected shortcuts keep theirs.
class ModifierState {
public:
    // A physical modifier event. `swallowed`: the engine swallowed it.
    void physical(uint16_t code, bool up, bool swallowed);
    // A modifier event we inject.
    void injected(uint16_t code, bool up);

    bool overriding() const { return !swallowed_.empty() || !injected_.empty(); }
    // `flags` with its modifier bits replaced by the modifiers held now.
    Flags apply(Flags flags) const;
    // For an event passed on: rewritten only while overriding().
    Flags passed(Flags flags) const { return overriding() ? apply(flags) : flags; }

private:
    std::set<uint16_t> held_, swallowed_, injected_;
};

// The event tap can't tell which keyboard an event came from, but IOHIDManager can. HID events are
// queued and matched to tap events by key and direction; unmatched ones fall back to the keyboard
// last pressed on. Not thread-safe: the caller locks.
class Attributor {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr size_t kMaxPending = 64;
    static constexpr std::chrono::milliseconds kTtl{500};

    void hid(unsigned short key, bool up, const std::string& device, Clock::time_point now);
    std::string attribute(unsigned short key, bool up, Clock::time_point now);
    const std::string& last() const { return last_; }

private:
    struct Event {
        unsigned short key;
        bool up;
        std::string device;
        Clock::time_point time;
    };
    std::deque<Event> pending_;
    std::string last_;
};

}  // namespace mac

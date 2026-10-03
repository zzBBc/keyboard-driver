// The macOS layer's decisions: key code tables, modifier flags, media keys, keyboard ids and which
// keyboard an event came from. Built on macOS only, like the rest of tests/macos/.
#include "harness.h"
#include "helpers.h"

#include "keycodes.h"
#include "mac_input.h"

#include <chrono>

namespace {

// macOS key codes used below (kVK_*).
constexpr uint16_t kA = 0x00, kTab = 0x30, kCapsLock = 0x39, kFn = 0x3F, kLeftArrow = 0x7B, kF1 = 0x7A,
                   kCommand = 0x37, kRightCommand = 0x36, kShift = 0x38, kControl = 0x3B, kRightControl = 0x3E,
                   kOption = 0x3A, kRightOption = 0x3D;

using mac::Flags;
using Clock = mac::Attributor::Clock;

}  // namespace

TEST(macKeyCodesMapToPortableIds) {
    CHECK(mac::keyFromMac(kA) == 'A');
    CHECK(mac::keyFromMac(kTab) == key::Tab);
    CHECK(mac::keyFromMac(kLeftArrow) == key::Left);
    CHECK(mac::keyFromMac(kF1) == key::F1);
    CHECK(mac::keyFromMac(kCommand) == key::LWin);
    CHECK(mac::keyFromMac(kRightCommand) == key::RWin);
    CHECK(mac::keyFromMac(kOption) == key::LAlt);
    CHECK(mac::keyFromMac(kRightOption) == key::RAlt);
    CHECK(mac::keyFromMac(kControl) == key::LControl);  // the sided id, not the either-side one
    CHECK(mac::keyFromMac(kRightControl) == key::RControl);
    CHECK(mac::keyFromMac(kShift) == key::LShift);
    CHECK(!mac::keyFromMac(kCapsLock));  // macOS toggles it itself: not remapped
    CHECK(!mac::keyFromMac(kFn));
}

TEST(macEveryKeyRoundTripsThroughItsCode) {
    for (unsigned short k = 1; k < 0x100; ++k) {
        const auto code = mac::macFromKey(k);
        if (!code) continue;
        const auto back = mac::keyFromMac(*code);
        CHECK(back.has_value());
        // Either-side modifiers come back as the left key.
        if (k == key::Ctrl) CHECK(back == key::LControl);
        else if (k == key::Shift) CHECK(back == key::LShift);
        else if (k == key::Alt) CHECK(back == key::LAlt);
        else CHECK(back == k);
    }
}

TEST(macEitherSideModifiersSendTheLeftKey) {
    CHECK(mac::macFromKey(key::Ctrl) == kControl);
    CHECK(mac::macFromKey(key::Shift) == kShift);
    CHECK(mac::macFromKey(key::Alt) == kOption);
    CHECK(mac::macFromKey(key::LWin) == kCommand);
}

TEST(macKeysWithoutAMacCodeMapToNothing) {
    CHECK(!mac::macFromKey(key::PrintScreen));
    CHECK(!mac::macFromKey(key::ScrollLock));
    CHECK(!mac::macFromKey(key::Pause));
    CHECK(!mac::macFromKey(key::F24));
    CHECK(!mac::macFromKey(key::BrowserBack));
    CHECK(!mac::macFromKey(key::LaunchMail));
    CHECK(!mac::macFromKey(key::CapsLock));
}

TEST(macHidUsagesMapToTheSameIdsAsKeyCodes) {
    CHECK(mac::keyFromHid(0x04) == 'A');
    CHECK(mac::keyFromHid(0x1E) == '1');
    CHECK(mac::keyFromHid(0x27) == '0');
    CHECK(mac::keyFromHid(0x2B) == key::Tab);
    CHECK(mac::keyFromHid(0x3A) == key::F1);
    CHECK(mac::keyFromHid(0x68) == key::F1 + 12);  // F13
    CHECK(mac::keyFromHid(0x49) == key::Insert);
    CHECK(mac::keyFromHid(0xE0) == key::LControl);
    CHECK(mac::keyFromHid(0xE3) == key::LWin);
    CHECK(mac::keyFromHid(0xE6) == key::RAlt);
    CHECK(!mac::keyFromHid(0x00));
    CHECK(!mac::keyFromHid(0x01));  // ErrorRollOver
    CHECK(!mac::keyFromHid(0x39));  // Caps Lock: not reported by the tap either
}

TEST(macModifierFlagsHaveGenericAndSideBits) {
    CHECK(mac::modifierFlags(kControl) == (mac::kFlagControl | 0x1));
    CHECK(mac::modifierFlags(kRightControl) == (mac::kFlagControl | 0x2000));
    CHECK(mac::modifierFlags(kShift) == (mac::kFlagShift | 0x2));
    CHECK(mac::modifierFlags(kCommand) == (mac::kFlagCommand | 0x8));
    CHECK(mac::modifierFlags(kRightOption) == (mac::kFlagAlternate | 0x40));
    CHECK(mac::modifierFlags(kA) == 0);
    CHECK(mac::modifierFlags(kCapsLock) == 0);
}

TEST(macModifierReleaseIsReadFromItsSideBit) {
    // Right Command pressed while Left Command stays held: the generic bit is set either way.
    const Flags both = mac::kFlagCommand | 0x8 | 0x10;
    const Flags leftOnly = mac::kFlagCommand | 0x8;
    CHECK(!mac::isModifierRelease(kRightCommand, both));
    CHECK(mac::isModifierRelease(kRightCommand, leftOnly));
    CHECK(!mac::isModifierRelease(kCommand, leftOnly));
    CHECK(mac::isModifierRelease(kCommand, 0));
}

TEST(macArrowAndFunctionKeysCarryTheirFlags) {
    CHECK(mac::keyFlags(kLeftArrow) == (mac::kFlagSecondaryFn | mac::kFlagNumericPad));
    CHECK(mac::keyFlags(kF1) == mac::kFlagSecondaryFn);
    CHECK(mac::keyFlags(kA) == 0);
}

TEST(macMediaKeysEncodeAndDecode) {
    const auto down = mac::decodeMedia(mac::encodeMedia(16, false));
    CHECK(down.nx == 16 && !down.up);
    const auto up = mac::decodeMedia(mac::encodeMedia(0, true));
    CHECK(up.nx == 0 && up.up);
    CHECK(mac::encodeMedia(16, false) == 0x100A00);  // NX_KEYTYPE_PLAY, key down
    CHECK(mac::decodeMedia(0x100A01).nx == 16 && !mac::decodeMedia(0x100A01).up);  // repeat bit ignored
}

TEST(macMediaKeysMapToPortableIds) {
    CHECK(mac::keyFromMedia(0) == key::VolumeUp);
    CHECK(mac::keyFromMedia(1) == key::VolumeDown);
    CHECK(mac::keyFromMedia(7) == key::VolumeMute);
    CHECK(mac::keyFromMedia(16) == key::MediaPlayPause);
    CHECK(mac::keyFromMedia(17) == key::MediaNext);
    CHECK(mac::keyFromMedia(19) == key::MediaNext);  // fast forward reads as next
    CHECK(mac::keyFromMedia(18) == key::MediaPrevious);
    CHECK(!mac::keyFromMedia(2));  // brightness
    CHECK(mac::mediaFromKey(key::MediaNext) == 17);  // sent as NEXT, not FAST
    CHECK(mac::mediaFromKey(key::VolumeUp) == 0);
    CHECK(!mac::mediaFromKey(key::MediaStop));
    CHECK(!mac::mediaFromKey('A'));
}

TEST(macDeviceIdMatchesTheWindowsFormat) {
    CHECK(mac::deviceId(0x046D, 0xC52B) == "VID_046D&PID_C52B");
    CHECK(mac::deviceId(0x5AC, 0x342) == "VID_05AC&PID_0342");
    CHECK(mac::deviceId(0, 0) == "UNKNOWN");
}

TEST(macFlagsAreLeftAloneWhileNothingIsOverridden) {
    mac::ModifierState s;
    s.physical(kCommand, false, false);  // passed through
    const Flags other = mac::kFlagShift | 0x2;  // e.g. another program's injected Shift+key
    CHECK(!s.overriding());
    CHECK(s.passed(other) == other);
}

TEST(macSwallowedModifierIsHiddenFromPassedEvents) {
    // ralt = rctrl: Right Option is swallowed, Right Control injected; then A is typed.
    mac::ModifierState s;
    s.physical(kRightOption, false, true);
    s.injected(kRightControl, false);
    CHECK(s.overriding());
    const Flags systemSays = mac::kFlagAlternate | 0x40 | mac::kFlagAlphaShift;  // the system still sees Option
    CHECK(s.passed(systemSays) == (mac::kFlagControl | 0x2000 | mac::kFlagAlphaShift));

    s.physical(kRightOption, true, true);  // its release is swallowed too
    s.injected(kRightControl, true);
    CHECK(!s.overriding());
}

TEST(macInjectedModifiersAddUpWithHeldOnes) {
    mac::ModifierState s;
    s.physical(kCommand, false, false);
    s.injected(kShift, false);
    CHECK(s.apply(0) == (mac::kFlagCommand | 0x8 | mac::kFlagShift | 0x2));
    CHECK(s.apply(mac::kFlagSecondaryFn) == (mac::kFlagSecondaryFn | mac::kFlagCommand | 0x8 | mac::kFlagShift | 0x2));
    s.injected(kShift, true);
    CHECK(!s.overriding());
    CHECK(s.apply(0) == (mac::kFlagCommand | 0x8));
}

TEST(macPhysicalReleaseEndsAnInjectedPressOfTheSameKey) {
    // ctrl+j = down while Ctrl is held: Ctrl is released for the step and pressed again after it.
    mac::ModifierState s;
    s.physical(kControl, false, false);
    s.injected(kControl, true);
    s.injected(kControl, false);
    CHECK(s.overriding());
    s.physical(kControl, true, false);
    CHECK(!s.overriding());
    CHECK(s.apply(0) == 0);
}

TEST(macPassedReleaseEndsASwallowedPress) {
    // The press was swallowed (a layer key) but the config changed before the release.
    mac::ModifierState s;
    s.physical(kOption, false, true);
    CHECK(s.overriding());
    s.physical(kOption, true, false);
    CHECK(!s.overriding());
}

TEST(macAttributorMatchesByKeyAndDirection) {
    mac::Attributor a;
    const auto t = Clock::now();
    a.hid('A', false, "K1", t);
    a.hid('B', false, "K2", t);
    CHECK(a.attribute('B', false, t) == "K2");
    CHECK(a.last() == "K2");
    a.hid('A', true, "K1", t);
    CHECK(a.attribute('A', true, t) == "K1");
}

TEST(macAttributorDropsOlderEntriesOnAMatch) {
    mac::Attributor a;
    const auto t = Clock::now();
    a.hid('A', false, "K1", t);
    a.hid('A', false, "K2", t);
    CHECK(a.attribute('A', false, t) == "K1");
    CHECK(a.attribute('A', false, t) == "K2");
    CHECK(a.attribute('A', false, t) == "K2");  // nothing left: the last keyboard pressed on
}

TEST(macAttributorFallsBackToTheLastKeyboard) {
    mac::Attributor a;
    const auto t = Clock::now();
    CHECK(a.attribute('A', false, t).empty());  // nothing known yet
    a.hid('A', false, "K1", t);
    a.hid('A', true, "K1", t);
    CHECK(a.attribute('Z', false, t) == "K1");  // unmatched
    CHECK(a.last() == "K1");                    // a release does not change the last keyboard
}

TEST(macAttributorForgetsStaleEntries) {
    mac::Attributor a;
    const auto t = Clock::now();
    a.hid('A', false, "K1", t);
    a.hid('B', false, "K2", t);
    CHECK(a.attribute('A', false, t + std::chrono::seconds(1)) == "K2");  // expired: falls back
}

TEST(macAttributorKeepsABoundedQueue) {
    mac::Attributor a;
    const auto t = Clock::now();
    a.hid('A', false, "OLD", t);
    for (size_t i = 0; i < mac::Attributor::kMaxPending; ++i) a.hid('B', false, "K2", t);
    CHECK(a.attribute('A', false, t) == "K2");  // the oldest entry was dropped
}

TEST(macCapsLockMappingsMoveToTheHidSystem) {
    KeyMap map{{key::Tab, key::CapsLock}, {key::CapsLock, key::Tab}, {'A', 'B'}};
    const auto hid = mac::takeCapsLockMappings(map);
    CHECK(hid.size() == 2);
    CHECK(hid[0] == (mac::HidMapping{0x70000002B, 0x700000039}));  // Tab -> Caps Lock
    CHECK(hid[1] == (mac::HidMapping{0x700000039, 0x70000002B}));  // Caps Lock -> Tab
    CHECK(map.size() == 1 && map.at('A') == 'B');                  // the tap still does the rest
}

TEST(macCapsLockToEitherSideModifierUsesTheLeftKey) {
    KeyMap map{{key::CapsLock, key::Ctrl}};
    const auto hid = mac::takeCapsLockMappings(map);
    CHECK(hid.size() == 1 && hid[0] == (mac::HidMapping{0x700000039, 0x7000000E0}));
    CHECK(map.empty());
}

TEST(macCapsLockToAKeyMacOsLacksStaysPut) {
    KeyMap map{{key::CapsLock, key::PrintScreen}};
    CHECK(mac::takeCapsLockMappings(map).empty());
    CHECK(map.size() == 1);
}

TEST(macUserKeyMappingJson) {
    CHECK(mac::userKeyMappingJson({}) == "{\"UserKeyMapping\":[]}");
    CHECK(mac::userKeyMappingJson({{0x700000039, 0x70000002B}}) ==
          "{\"UserKeyMapping\":[{\"HIDKeyboardModifierMappingSrc\":30064771129,"
          "\"HIDKeyboardModifierMappingDst\":30064771115}]}");
}

TEST(macActionListSendsOnlyKeysMacOSHas) {
    // Every step of the shipped macOS actions must be a key macOS can post (a key code or a media
    // event); a key without one would make the action do nothing.
    auto p = parse(sourceFile("config/actions.macos.txt"));
    CHECK(p.ok && !p.cfg.actions.empty());
    for (const auto& [name, steps] : p.cfg.actions)
        for (const auto& step : steps) {
            const bool sendable = mac::macFromKey(step.key) || mac::mediaFromKey(step.key);
            if (!sendable) std::cerr << "  " << name << ": key " << step.key << " has no macOS code\n";
            CHECK(sendable);
        }
}

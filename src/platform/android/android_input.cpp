#include "android_input.h"

#include "keycodes.h"

namespace android {

namespace {

struct Pair {
    int android;
    unsigned short id;
};

// Android key codes that are not a run of consecutive codes.
constexpr Pair kKeys[] = {
    {KeycodeBack, key::BrowserBack},       {KeycodeHome, key::BrowserHome},
    {KeycodeForward, key::BrowserForward}, {KeycodeEnvelope, key::LaunchMail},
    {19, key::Up},          {20, key::Down},           {21, key::Left},        {22, key::Right},
    {24, key::VolumeUp},    {25, key::VolumeDown},     {164, key::VolumeMute},
    {55, key::Comma},       {56, key::Period},         {57, key::LAlt},        {58, key::RAlt},
    {59, key::LShift},      {60, key::RShift},         {61, key::Tab},         {62, key::Space},
    {66, key::Enter},       {67, key::Backspace},      {68, key::Backquote},   {69, key::Minus},
    {70, key::Equals},      {71, key::LeftBracket},    {72, key::RightBracket}, {73, key::Backslash},
    {74, key::Semicolon},   {75, key::Quote},          {76, key::Slash},       {82, key::Apps},
    {85, key::MediaPlayPause}, {86, key::MediaStop},   {87, key::MediaNext},   {88, key::MediaPrevious},
    {92, key::PageUp},      {93, key::PageDown},       {111, key::Escape},     {112, key::Delete},
    {113, key::LControl},   {114, key::RControl},      {115, key::CapsLock},   {116, key::ScrollLock},
    {117, key::LWin},       {118, key::RWin},          {120, key::PrintScreen}, {121, key::Pause},
    {122, key::Home},       {123, key::End},           {124, key::Insert},     {143, key::NumLock},
    {KeycodeAppSwitch, key::Apps},
};

}  // namespace

unsigned short fromAndroid(int keyCode) {
    if (keyCode >= 7 && keyCode <= 16) return static_cast<unsigned short>('0' + (keyCode - 7));    // KEYCODE_0..9
    if (keyCode >= 29 && keyCode <= 54) return static_cast<unsigned short>('A' + (keyCode - 29));  // KEYCODE_A..Z
    if (keyCode >= 131 && keyCode <= 142) return static_cast<unsigned short>(key::F1 + (keyCode - 131));  // F1..F12
    for (const Pair& p : kKeys)
        if (p.android == keyCode) return p.id;
    return 0;
}

int globalActionFor(unsigned short k) {
    switch (k) {
        case key::BrowserBack: return ActionBack;
        case key::BrowserHome: return ActionHome;
        case key::Apps: return ActionRecents;
        case key::LaunchMail: return ActionNotifications;
        case key::BrowserRefresh: return ActionQuickSettings;
        case key::ScrollLock: return ActionPowerDialog;
        case key::Pause: return ActionLockScreen;
        case key::PrintScreen: return ActionTakeScreenshot;
        default: return 0;
    }
}

Decision decide(bool swallow, const std::vector<KeyEvent>& send) {
    Decision d;
    if (!swallow) return d;  // the engine left the key alone; whatever it would inject is not possible anyway
    bool sawAction = false;
    for (const KeyEvent& e : send) {
        const int action = globalActionFor(e.key);
        if (action) {
            sawAction = true;
            if (!e.up) d.actions.push_back(action);  // a global action runs once, on the press
        } else if (!modBit(e.key)) {
            return Decision{};  // a key we cannot send: keep the physical key instead of losing it
        }
    }
    // Modifier events around an action (the engine releases and restores held ones) can be dropped, but
    // a mapping that sends only modifiers has nothing to carry out.
    if (!send.empty() && !sawAction) return Decision{};
    d.swallow = true;
    return d;
}

}  // namespace android

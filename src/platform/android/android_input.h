#pragma once
// The Android layer's decisions, with no JNI or Android API so tests/android/ can run them on any
// host: Android key codes <-> portable key ids, and what the accessibility service can do with the
// engine's answer.
//
// An accessibility service sees hardware-keyboard events and can swallow them, but cannot inject
// keys. What it can do is run a global action (Back, Home, ...). So the engine's output is only
// carried out when every key it asks for is one of the keys below that stand for a global action;
// anything else lets the original key through untouched.
#include <vector>

#include "matcher.h"

namespace android {

// KeyEvent.KEYCODE_* values the layer relies on (android.view.KeyEvent).
enum : int {
    KeycodeHome = 3,
    KeycodeBack = 4,
    KeycodeEnvelope = 65,
    KeycodeForward = 125,
    KeycodeAppSwitch = 187,
};

// AccessibilityService.GLOBAL_ACTION_* values.
enum : int {
    ActionBack = 1,
    ActionHome = 2,
    ActionRecents = 3,
    ActionNotifications = 4,
    ActionQuickSettings = 5,
    ActionPowerDialog = 6,
    ActionLockScreen = 8,
    ActionTakeScreenshot = 9,
};

// Portable key id for an Android key code, 0 for a key with no id (it is then never remapped).
unsigned short fromAndroid(int keyCode);

// The global action a portable key stands for when the engine sends it, 0 if it stands for none.
// browserback = Back, browserhome = Home, apps = Recents, launchmail = Notifications,
// browserrefresh = Quick settings, scrolllock = Power menu, pause = Lock screen,
// printscreen = Screenshot.
int globalActionFor(unsigned short key);

struct Decision {
    bool swallow = false;
    std::vector<int> actions;  // global actions to run, in order
};

// Turns the engine's answer for one physical event into what the service does: swallow it and run
// the global actions, or let it through. `send` holds the key events the engine wants injected.
Decision decide(bool swallow, const std::vector<KeyEvent>& send);

}  // namespace android

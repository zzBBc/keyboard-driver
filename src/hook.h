#pragma once
#include <memory>
#include <string>
#include <vector>

#include "config_loader.h"

// Replace the config for one keyboard (deviceId, e.g. "VID_046D&PID_C52B"), or the default
// used by keyboards without their own config (empty deviceId). A null cfg removes a
// keyboard's own config so it falls back to the default.
// Thread-safe; takes effect on the next key event.
void setConfig(const std::string& deviceId, std::shared_ptr<const Config> cfg);

struct KeyboardInfo {
    std::string id;    // hardware id, same format as the device config file names
    std::string name;  // product name if the device reports one, else the id
};

// Keyboards currently attached (one entry per hardware id).
std::vector<KeyboardInfo> listKeyboards();

// Hardware id of the keyboard that produced the most recent key press ("" if none yet).
std::string lastKeyboard();

// Install the low-level keyboard hook and run the message loop until WM_QUIT.
// Returns the process exit code.
int runHook();

#pragma once
// What the program needs from the operating system. Each supported OS provides one implementation of
// this (platform/windows/, platform/macos/). Everything else (config, engine, server, GUI) is portable.
#include <string>
#include <vector>

class Engine;
struct Config;

struct KeyboardInfo {
    std::string id;    // hardware id, same format as the device config file names
    std::string name;  // product name if the device reports one, else the id
};

namespace platform {

// "windows" or "macos". The GUI uses it to name keys and draw the keyboard the way this OS does.
const char* name();

// Folder that holds the executable and its data files (web/, actions.txt, mappings.default.txt).
std::string exeDir();

// Keyboards currently attached (one entry per hardware id).
std::vector<KeyboardInfo> listKeyboards();

// Hardware id of the keyboard that produced the most recent key press ("" if none yet).
std::string lastKeyboard();

// Makes sure only one copy of the program runs. If another copy is already running, asks it to quit and
// waits (a few seconds) for it to go, so this copy can take over its port and keyboard hook. Returns
// false if the other copy did not go away. Call this first thing at start-up.
bool takeOverFromRunningInstance();

// Opens `url` in the user's default browser. Returns false if it could not be launched.
bool openUrl(const std::string& url);

// Called with each config before the engine gets it (deviceId "" = the default config). The layer may
// carry out some mappings itself, below its hook, and remove them from `cfg` (macOS: Caps Lock ones).
// Safe to call from any thread.
void adoptConfig(const std::string& deviceId, Config& cfg);

// Ask the running keyboard hook to stop, so runHook() returns. Safe to call from any thread, and
// before runHook() has started (then it returns as soon as it starts).
void requestQuit();

// Start intercepting keyboard input: report each physical key event to `engine` (with the keyboard it
// came from) and inject the key events it asks for. Blocks until the program should exit (Ctrl+C) and
// returns the exit code. Injected events must never be reported back to the engine.
int runHook(Engine& engine);

}  // namespace platform

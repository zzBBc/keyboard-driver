#pragma once
// What the program needs from the operating system. Each supported OS provides one implementation of
// this (platform/windows/ today). Everything else (config, engine, server, GUI) is portable.
#include <string>
#include <vector>

class Engine;

struct KeyboardInfo {
    std::string id;    // hardware id, same format as the device config file names
    std::string name;  // product name if the device reports one, else the id
};

namespace platform {

// Folder that holds the executable and its data files (web/, actions.txt, mappings.default.txt).
std::string exeDir();

// Keyboards currently attached (one entry per hardware id).
std::vector<KeyboardInfo> listKeyboards();

// Hardware id of the keyboard that produced the most recent key press ("" if none yet).
std::string lastKeyboard();

// Start intercepting keyboard input: report each physical key event to `engine` (with the keyboard it
// came from) and inject the key events it asks for. Blocks until the program should exit (Ctrl+C) and
// returns the exit code. Injected events must never be reported back to the engine.
int runHook(Engine& engine);

}  // namespace platform

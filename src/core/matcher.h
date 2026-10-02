#pragma once
#include <vector>

#include "config_loader.h"

// Pure key-matching and expansion logic, kept apart from the Windows hook so it can be tested.

struct KeyEvent {
    unsigned short key;
    bool up;
};

// Mod bit for a modifier virtual key (either side), 0 for anything else.
unsigned modBit(unsigned short vk);

// Combined mod bits of the physically held modifier keys.
unsigned modsOf(const std::vector<unsigned short>& heldModKeys);

// The binding whose chord is exactly `mods` + `vk` (no extra, no missing modifiers), or null.
const Binding* findBinding(const std::vector<Binding>& list, unsigned mods, unsigned short vk);

// Key events that perform `steps` while `heldModKeys` are physically down. Each step makes the
// modifier state match the step (releasing extra held modifiers, pressing missing ones), taps the
// key, then puts the modifiers back as they were. A modifier that is already held and wanted
// (Alt for Alt+Tab) is left alone, so the Windows switcher stays open while Alt is held.
std::vector<KeyEvent> expand(const Steps& steps, const std::vector<unsigned short>& heldModKeys);

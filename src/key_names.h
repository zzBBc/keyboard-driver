#pragma once
#include <optional>
#include <string>
#include <vector>

// Resolve a key name ("CapsLock", "A", "F5", ...) to a Windows virtual-key code.
// Shortest name for a virtual key ("esc" rather than "escape"), "" if it has none.
std::string nameFromVk(unsigned short vk);

std::optional<unsigned short> vkFromName(const std::string& name);

// All accepted key names, sorted. Used by the web GUI for autocomplete.
std::vector<std::string> allKeyNames();

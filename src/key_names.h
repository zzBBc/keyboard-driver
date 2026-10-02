#pragma once
#include <optional>
#include <string>
#include <vector>

// Resolve a key name ("CapsLock", "A", "F5", ...) to a Windows virtual-key code.
std::optional<unsigned short> vkFromName(const std::string& name);

// All accepted key names, sorted. Used by the web GUI for autocomplete.
std::vector<std::string> allKeyNames();

#pragma once
// Switching the macOS input source, as the Globe key does. Kept apart from the config types: Carbon,
// which has the Text Input Sources API, defines a KeyMap of its own.
#include <optional>
#include <string>
#include <vector>

namespace mac {

// An enabled input source as Text Input Sources reports it. `keyboard`: in the keyboard category
// (not e.g. the Character Viewer); `selectable`: can be chosen directly (an input method with modes
// is chosen through its modes).
struct InputSource {
    std::string id;
    bool keyboard;
    bool selectable;
};

// The source the Globe key would switch to from `current`: the next selectable keyboard source in
// the list, wrapping around (the first one if `current` isn't listed). None if there is no other.
std::optional<std::string> nextInputSource(const std::vector<InputSource>& enabled, const std::string& current);

// Select the source nextInputSource picks, with Text Input Sources. Main thread only.
void selectNextInputSource();

}  // namespace mac

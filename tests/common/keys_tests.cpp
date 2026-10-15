// Key names and ids.
#include "harness.h"
#include "helpers.h"

#include "key_names.h"
#include "keycodes.h"

TEST(keyNameFromVk) {
    CHECK(nameFromVk(key::Tab) == "tab");
    CHECK(nameFromVk(key::Escape) == "esc");  // shortest of esc/escape
    CHECK(nameFromVk(key::Enter) == "enter");
    CHECK(nameFromVk(key::Period) == ".");
    CHECK(nameFromVk('Q') == "q");
    CHECK(nameFromVk(key::F4) == "f4");
    CHECK(nameFromVk(0x07).empty());  // undefined key
}

TEST(mediaKeyNames) {
    CHECK(vkFromName("volumeup").value_or(0) == key::VolumeUp);
    CHECK(vkFromName("volumedown").value_or(0) == key::VolumeDown);
    CHECK(vkFromName("mute").value_or(0) == key::VolumeMute);
    CHECK(vkFromName("playpause").value_or(0) == key::MediaPlayPause);
    CHECK(vkFromName("nexttrack").value_or(0) == key::MediaNext);
    CHECK(vkFromName("previoustrack").value_or(0) == key::MediaPrevious);
    CHECK(vkFromName("browserback").value_or(0) == key::BrowserBack);
    CHECK(nameFromVk(key::VolumeUp) == "volumeup");  // reverse lookup works for them too
}

TEST(inputSourceKeyName) {
    // Not a key on any keyboard: sending it switches to the next input source (macOS).
    CHECK(vkFromName("inputsource").value_or(0) == key::InputSource);
    CHECK(nameFromVk(key::InputSource) == "inputsource");
}

#pragma once
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config_loader.h"
#include "matcher.h"

// The platform-independent core of the remapper: given each physical key event and the keyboard it
// came from, decide whether to swallow it and which key events to inject instead. The platform layer
// (Windows hook, macOS event tap, ...) reports events and performs the injections.
class Engine {
public:
    // Replace the config for one keyboard (deviceId, e.g. "VID_046D&PID_C52B"), or the default used by
    // keyboards without their own config (empty deviceId). A null cfg removes a keyboard's own config
    // so it falls back to the default. Thread-safe; takes effect on the next key event.
    void setConfig(const std::string& deviceId, std::shared_ptr<const Config> cfg);

    // One physical (not injected) key event. Returns true to swallow it. `send` receives the key
    // events to inject, in order. Must be called from one thread only (the platform's hook thread).
    bool handle(const std::string& device, unsigned short key, bool up, std::vector<KeyEvent>& send);

private:
    struct Profiles {
        std::shared_ptr<const Config> def = std::make_shared<const Config>();
        std::map<std::string, std::shared_ptr<const Config>> devices;
    };
    // Per keyboard: what is held right now.
    struct DeviceState {
        std::unordered_set<unsigned short> heldTriggers;  // layer keys currently down
        // physical key -> key we sent for it (0 = swallowed, e.g. it ran a chord binding)
        std::unordered_map<unsigned short, unsigned short> down;
        std::vector<unsigned short> heldMods;  // modifier keys physically down
    };

    static bool isTrigger(const Config& cfg, unsigned short key);
    static const unsigned short* resolve(const Config& cfg, const DeviceState& st, unsigned short key);
    static const Binding* resolveChord(const Config& cfg, const DeviceState& st, unsigned short key);

    std::shared_ptr<const Profiles> profiles_ = std::make_shared<const Profiles>();
    std::mutex profilesMutex_;  // serialises setConfig; handle() just reads a snapshot
    std::map<std::string, DeviceState> state_;
};

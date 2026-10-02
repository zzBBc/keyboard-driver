#include "engine.h"

#include <algorithm>
#include <atomic>

void Engine::setConfig(const std::string& deviceId, std::shared_ptr<const Config> cfg) {
    std::lock_guard<std::mutex> lock(profilesMutex_);
    auto next = std::make_shared<Profiles>(*std::atomic_load(&profiles_));
    if (deviceId.empty()) {
        if (cfg) next->def = std::move(cfg);
    } else if (cfg) {
        next->devices[deviceId] = std::move(cfg);
    } else {
        next->devices.erase(deviceId);
    }
    std::atomic_store(&profiles_, std::shared_ptr<const Profiles>(std::move(next)));
}

bool Engine::isTrigger(const Config& cfg, unsigned short key) {
    for (const auto& layer : cfg.layers)
        if (layer.trigger == key) return true;
    return false;
}

// Active layers first (in config order), then the base map.
const unsigned short* Engine::resolve(const Config& cfg, const DeviceState& st, unsigned short key) {
    for (const auto& layer : cfg.layers) {
        if (!st.heldTriggers.count(layer.trigger)) continue;
        if (auto it = layer.map.find(key); it != layer.map.end()) return &it->second;
    }
    if (auto it = cfg.base.map.find(key); it != cfg.base.map.end()) return &it->second;
    return nullptr;
}

// Binding for this chord: active layers first (in config order), then the base.
const Binding* Engine::resolveChord(const Config& cfg, const DeviceState& st, unsigned short key) {
    const unsigned mods = modsOf(st.heldMods);
    for (const auto& layer : cfg.layers) {
        if (!st.heldTriggers.count(layer.trigger)) continue;
        if (const Binding* b = findBinding(layer.chords, mods, key)) return b;
    }
    return findBinding(cfg.base.chords, mods, key);
}

bool Engine::handle(const std::string& device, unsigned short key, bool up, std::vector<KeyEvent>& send) {
    const auto profiles = std::atomic_load(&profiles_);
    DeviceState& st = state_[device];

    if (modBit(key)) {  // track physical modifiers for chord bindings
        auto& mods = st.heldMods;
        auto it = std::find(mods.begin(), mods.end(), key);
        if (up && it != mods.end()) mods.erase(it);
        else if (!up && it == mods.end()) mods.push_back(key);
    }
    const auto own = profiles->devices.find(device);
    const Config& cfg = *(own != profiles->devices.end() ? own->second : profiles->def);

    if (up) {
        // Checked even if the config changed meanwhile, so nothing gets stuck. If the platform guessed the
        // wrong keyboard, the key may be held under another one, so look there too.
        auto release = [&](DeviceState& s) {
            if (s.heldTriggers.erase(key)) return true;
            if (auto it = s.down.find(key); it != s.down.end()) {
                if (it->second) send.push_back({it->second, true});  // release what we pressed, even if the layer is gone
                s.down.erase(it);
                return true;
            }
            return false;
        };
        if (release(st)) return true;
        for (auto& [id, other] : state_)
            if (&other != &st && release(other)) return true;
        return false;
    }

    if (isTrigger(cfg, key)) {  // also swallows auto-repeat
        st.heldTriggers.insert(key);
        return true;
    }
    if (auto it = st.down.find(key); it != st.down.end()) {  // auto-repeat of a remapped key
        if (it->second) send.push_back({it->second, false});
        return true;
    }
    if (!modBit(key)) {
        if (const Binding* b = resolveChord(cfg, st, key)) {
            const auto events = expand(b->out, st.heldMods);
            send.insert(send.end(), events.begin(), events.end());
            st.down[key] = 0;  // swallow the trigger key's release (and auto-repeat) too
            return true;
        }
    }
    if (const unsigned short* to = resolve(cfg, st, key)) {
        st.down[key] = *to;
        send.push_back({*to, false});
        return true;
    }
    return false;
}

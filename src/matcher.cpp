#include "matcher.h"

#include "keycodes.h"

namespace {

constexpr unsigned kAllMods[] = {ModCtrl, ModAlt, ModShift, ModWin};

// Key we press when a modifier has to be held and the user isn't holding one.
unsigned short leftKeyFor(unsigned mod) {
    switch (mod) {
        case ModCtrl: return key::LControl;
        case ModAlt: return key::LAlt;
        case ModShift: return key::LShift;
        default: return key::LWin;
    }
}

}  // namespace

unsigned modBit(unsigned short vk) {
    switch (vk) {
        case key::LControl: case key::RControl: case key::Ctrl: return ModCtrl;
        case key::LAlt: case key::RAlt: case key::Alt: return ModAlt;
        case key::LShift: case key::RShift: case key::Shift: return ModShift;
        case key::LWin: case key::RWin: return ModWin;
        default: return 0;
    }
}

unsigned modsOf(const std::vector<unsigned short>& heldModKeys) {
    unsigned mods = 0;
    for (unsigned short vk : heldModKeys) mods |= modBit(vk);
    return mods;
}

const Binding* findBinding(const std::vector<Binding>& list, unsigned mods, unsigned short vk) {
    for (const auto& b : list)
        if (!b.out.empty() && b.from.mods == mods && b.from.key == vk) return &b;
    return nullptr;
}

std::vector<KeyEvent> expand(const Steps& steps, const std::vector<unsigned short>& heldModKeys) {
    std::vector<KeyEvent> out;
    const unsigned held = modsOf(heldModKeys);

    for (const KeyChord& step : steps) {
        std::vector<unsigned short> released;  // physically held, not wanted by this step
        for (unsigned short vk : heldModKeys)
            if (!(modBit(vk) & step.mods)) {
                out.push_back({vk, true});
                released.push_back(vk);
            }

        std::vector<unsigned short> pressed;  // wanted, not physically held
        for (unsigned mod : kAllMods)
            if ((step.mods & mod) && !(held & mod)) {
                out.push_back({leftKeyFor(mod), false});
                pressed.push_back(leftKeyFor(mod));
            }

        out.push_back({step.key, false});
        out.push_back({step.key, true});

        for (unsigned short vk : pressed) out.push_back({vk, true});
        for (unsigned short vk : released) out.push_back({vk, false});
    }
    return out;
}

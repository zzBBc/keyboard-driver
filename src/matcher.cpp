#include "matcher.h"

#include <windows.h>

namespace {

constexpr unsigned kAllMods[] = {ModCtrl, ModAlt, ModShift, ModWin};

// Key we press when a modifier has to be held and the user isn't holding one.
unsigned short leftKeyFor(unsigned mod) {
    switch (mod) {
        case ModCtrl: return VK_LCONTROL;
        case ModAlt: return VK_LMENU;
        case ModShift: return VK_LSHIFT;
        default: return VK_LWIN;
    }
}

}  // namespace

unsigned modBit(unsigned short vk) {
    switch (vk) {
        case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL: return ModCtrl;
        case VK_LMENU: case VK_RMENU: case VK_MENU: return ModAlt;
        case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT: return ModShift;
        case VK_LWIN: case VK_RWIN: return ModWin;
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

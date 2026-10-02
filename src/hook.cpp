#include "hook.h"

#include "matcher.h"

#include <windows.h>
#include <hidsdi.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <iostream>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace {

// Tag on events we inject, so the hook never remaps its own output.
constexpr ULONG_PTR kInjectedTag = 0x4B4D4150;  // 'KMAP'

// The low-level hook can't tell which keyboard an event came from, but Raw Input can. Raw Input
// events are queued here and matched to hook events by scan code and direction.
struct RawEvent {
    unsigned scan;  // make code | 0x100 if extended
    bool up;
    std::string device;
    DWORD tick;
};
constexpr size_t kMaxPending = 64;
constexpr DWORD kPendingTtlMs = 500;

struct Profiles {
    std::shared_ptr<const Config> def = std::make_shared<const Config>();
    std::map<std::string, std::shared_ptr<const Config>> devices;
};

std::shared_ptr<const Profiles> g_profiles = std::make_shared<const Profiles>();
std::mutex g_profilesMutex;  // serialises writers; readers just atomic_load

HHOOK g_hook = nullptr;
HWND g_rawWindow = nullptr;
DWORD g_mainThread = 0;

std::mutex g_lastMutex;
std::string g_lastDevice;

// Hook-thread-only state.
struct DeviceState {
    std::unordered_set<unsigned short> heldTriggers;          // layer keys currently down
    // physical key -> key we sent for it (0 = swallowed, e.g. it ran a chord binding)
    std::unordered_map<unsigned short, unsigned short> down;
    std::vector<unsigned short> heldMods;  // modifier keys physically down
};
std::map<std::string, DeviceState> g_state;
std::deque<RawEvent> g_pending;
std::unordered_map<HANDLE, std::string> g_handleIds;  // raw input device handle -> hardware id

// "\\?\HID#VID_046D&PID_C52B&MI_00#7&..." -> "VID_046D&PID_C52B"; other buses use the hardware id
// segment, e.g. "\\?\ACPI#PNP0303#..." -> "PNP0303".
std::string deviceIdFromPath(const std::wstring& wpath) {
    std::string path;
    for (wchar_t c : wpath) path += static_cast<char>(c < 128 ? std::toupper(c) : '?');

    if (auto v = path.find("VID_"); v != std::string::npos) {
        auto p = path.find("&PID_", v);
        if (p != std::string::npos && path.size() >= p + 9) return path.substr(v, p + 9 - v);
    }
    auto a = path.find('#');
    if (a != std::string::npos) {
        auto b = path.find_first_of("#&", a + 1);
        std::string seg = path.substr(a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
        if (validDeviceId(seg)) return seg;
    }
    return "UNKNOWN";
}

std::wstring rawDevicePath(HANDLE h) {
    UINT n = 0;
    GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, nullptr, &n);
    std::wstring name(n, L'\0');
    if (n == 0 || GetRawInputDeviceInfoW(h, RIDI_DEVICENAME, name.data(), &n) == static_cast<UINT>(-1)) return L"";
    name.resize(wcslen(name.c_str()));
    return name;
}

std::string idForHandle(HANDLE h) {
    if (auto it = g_handleIds.find(h); it != g_handleIds.end()) return it->second;
    return g_handleIds[h] = deviceIdFromPath(rawDevicePath(h));
}

bool isExtendedKey(WORD vk) {
    switch (vk) {
        case VK_RCONTROL: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_APPS:
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT:
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_NUMLOCK: case VK_SNAPSHOT:
            return true;
        default:
            return false;
    }
}

void sendKey(WORD vk, bool keyUp) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (keyUp ? KEYEVENTF_KEYUP : 0) | (isExtendedKey(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    in.ki.dwExtraInfo = kInjectedTag;
    SendInput(1, &in, sizeof(INPUT));
}

bool isTrigger(const Config& cfg, unsigned short vk) {
    for (const auto& layer : cfg.layers)
        if (layer.trigger == vk) return true;
    return false;
}

// Active layers first (in config order), then the base map.
const unsigned short* resolve(const Config& cfg, const DeviceState& st, unsigned short vk) {
    for (const auto& layer : cfg.layers) {
        if (!st.heldTriggers.count(layer.trigger)) continue;
        if (auto it = layer.map.find(vk); it != layer.map.end()) return &it->second;
    }
    if (auto it = cfg.base.map.find(vk); it != cfg.base.map.end()) return &it->second;
    return nullptr;
}

// Binding for this chord: active layers first (in config order), then the base.
const Binding* resolveChord(const Config& cfg, const DeviceState& st, unsigned short vk) {
    const unsigned mods = modsOf(st.heldMods);
    for (const auto& layer : cfg.layers) {
        if (!st.heldTriggers.count(layer.trigger)) continue;
        if (const Binding* b = findBinding(layer.chords, mods, vk)) return b;
    }
    return findBinding(cfg.base.chords, mods, vk);
}

// Which keyboard produced this hook event? Falls back to the last known one if Raw Input
// hasn't reported it (yet).
std::string attribute(const KBDLLHOOKSTRUCT& kb, bool up) {
    const unsigned scan = kb.scanCode | ((kb.flags & LLKHF_EXTENDED) ? 0x100u : 0u);
    const DWORD now = GetTickCount();
    while (!g_pending.empty() && now - g_pending.front().tick > kPendingTtlMs) g_pending.pop_front();

    for (auto it = g_pending.begin(); it != g_pending.end(); ++it) {
        if (it->scan == scan && it->up == up) {
            std::string id = it->device;
            g_pending.erase(g_pending.begin(), it + 1);  // older entries are stale
            return id;
        }
    }
    std::lock_guard<std::mutex> lock(g_lastMutex);
    return g_lastDevice;
}

LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION) return CallNextHookEx(g_hook, code, wParam, lParam);

    const auto* kb = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
    if (kb->dwExtraInfo == kInjectedTag) return CallNextHookEx(g_hook, code, wParam, lParam);

    const auto profiles = std::atomic_load(&g_profiles);
    const auto vk = static_cast<unsigned short>(kb->vkCode);
    const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    const std::string dev = attribute(*kb, up);
    DeviceState& st = g_state[dev];

    if (modBit(vk)) {  // track physical modifiers for chord bindings
        auto& mods = st.heldMods;
        auto it = std::find(mods.begin(), mods.end(), vk);
        if (up && it != mods.end()) mods.erase(it);
        else if (!up && it == mods.end()) mods.push_back(vk);
    }
    const Config& cfg = *(profiles->devices.count(dev) ? profiles->devices.at(dev) : profiles->def);

    if (up) {
        // Checked even if the config changed meanwhile, so nothing gets stuck. If attribution
        // was wrong, the key may be held under another keyboard, so look there too.
        auto release = [&](DeviceState& s) {
            if (s.heldTriggers.erase(vk)) return true;
            if (auto it = s.down.find(vk); it != s.down.end()) {
                if (it->second) sendKey(it->second, true);  // release what we pressed, even if the layer is gone
                s.down.erase(it);
                return true;
            }
            return false;
        };
        if (release(st)) return 1;
        for (auto& [id, other] : g_state)
            if (&other != &st && release(other)) return 1;
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    if (isTrigger(cfg, vk)) {  // also swallows auto-repeat
        st.heldTriggers.insert(vk);
        return 1;
    }
    if (auto it = st.down.find(vk); it != st.down.end()) {  // auto-repeat of a remapped key
        if (it->second) sendKey(it->second, false);
        return 1;
    }
    if (!modBit(vk)) {
        if (const Binding* b = resolveChord(cfg, st, vk)) {
            for (const KeyEvent& e : expand(b->out, st.heldMods)) sendKey(e.vk, e.up);
            st.down[vk] = 0;  // swallow the trigger key's release (and auto-repeat) too
            return 1;
        }
    }
    if (const unsigned short* to = resolve(cfg, st, vk)) {
        st.down[vk] = *to;
        sendKey(*to, false);
        return 1;
    }
    return CallNextHookEx(g_hook, code, wParam, lParam);
}

LRESULT CALLBACK rawWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT_DEVICE_CHANGE) {
        g_handleIds.clear();
        return 0;
    }
    if (msg == WM_INPUT) {
        alignas(8) BYTE buf[256];
        UINT size = sizeof buf;
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buf, &size,
                            sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1)) {
            const auto* ri = reinterpret_cast<const RAWINPUT*>(buf);
            // hDevice is null for input we inject ourselves; VKey 0xFF marks fake shift events.
            if (ri->header.dwType == RIM_TYPEKEYBOARD && ri->header.hDevice && ri->data.keyboard.VKey != 0xFF) {
                const auto& k = ri->data.keyboard;
                RawEvent e{k.MakeCode | ((k.Flags & RI_KEY_E0) ? 0x100u : 0u), (k.Flags & RI_KEY_BREAK) != 0,
                           idForHandle(ri->header.hDevice), GetTickCount()};
                if (!e.up) {
                    std::lock_guard<std::mutex> lock(g_lastMutex);
                    g_lastDevice = e.device;
                }
                g_pending.push_back(std::move(e));
                if (g_pending.size() > kMaxPending) g_pending.pop_front();
            }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);  // required for WM_INPUT cleanup
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool createRawInputWindow() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = rawWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"KeymapperRawInput";
    if (!RegisterClassW(&wc)) return false;
    g_rawWindow = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!g_rawWindow) return false;

    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;  // generic desktop
    rid.usUsage = 0x06;      // keyboard
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;  // receive even when not focused
    rid.hwndTarget = g_rawWindow;
    return RegisterRawInputDevices(&rid, 1, sizeof rid) != FALSE;
}

// Console control handlers run on a separate thread, so post WM_QUIT to the hook thread.
BOOL WINAPI quitHandler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT) {
        PostThreadMessageW(g_mainThread, WM_QUIT, 0, 0);
        return TRUE;
    }
    return FALSE;
}

}  // namespace

void setConfig(const std::string& deviceId, std::shared_ptr<const Config> cfg) {
    std::lock_guard<std::mutex> lock(g_profilesMutex);
    auto next = std::make_shared<Profiles>(*std::atomic_load(&g_profiles));
    if (deviceId.empty()) {
        if (cfg) next->def = std::move(cfg);
    } else if (cfg) {
        next->devices[deviceId] = std::move(cfg);
    } else {
        next->devices.erase(deviceId);
    }
    std::atomic_store(&g_profiles, std::shared_ptr<const Profiles>(std::move(next)));
}

std::vector<KeyboardInfo> listKeyboards() {
    std::vector<KeyboardInfo> out;
    UINT n = 0;
    GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST));
    std::vector<RAWINPUTDEVICELIST> list(n);
    if (n == 0 || GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1)) return out;

    for (UINT i = 0; i < n; ++i) {
        if (list[i].dwType != RIM_TYPEKEYBOARD) continue;
        const std::wstring path = rawDevicePath(list[i].hDevice);
        const std::string id = deviceIdFromPath(path);
        if (std::any_of(out.begin(), out.end(), [&](const KeyboardInfo& k) { return k.id == id; })) continue;

        std::string name = id;
        HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            wchar_t product[126] = {};
            if (HidD_GetProductString(h, product, sizeof product) && product[0]) {
                name.clear();
                for (const wchar_t* c = product; *c; ++c) name += static_cast<char>(*c < 128 ? *c : '?');
            }
            CloseHandle(h);
        }
        out.push_back({id, name});
    }
    return out;
}

std::string lastKeyboard() {
    std::lock_guard<std::mutex> lock(g_lastMutex);
    return g_lastDevice;
}

int runHook() {
    g_mainThread = GetCurrentThreadId();

    if (!createRawInputWindow())
        std::cerr << "Raw Input unavailable (" << GetLastError() << "); using the default config for every keyboard.\n";

    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, lowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    if (!g_hook) {
        std::cerr << "SetWindowsHookEx failed: " << GetLastError() << "\n";
        return 1;
    }
    SetConsoleCtrlHandler(quitHandler, TRUE);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(g_hook);
    return 0;
}

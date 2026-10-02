#include "platform.h"

#include "engine.h"
#include "keycodes.h"
#include "matcher.h"

#include <windows.h>
#include <hidsdi.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <mutex>
#include <unordered_map>

// The core uses portable key ids (keycodes.h) whose values equal the Windows virtual-key codes, so the
// hook passes them straight through. These guard that assumption.
static_assert(key::Backspace == VK_BACK, "key id Backspace must equal VK_BACK");
static_assert(key::Tab == VK_TAB, "key id Tab must equal VK_TAB");
static_assert(key::Enter == VK_RETURN, "key id Enter must equal VK_RETURN");
static_assert(key::Escape == VK_ESCAPE, "key id Escape must equal VK_ESCAPE");
static_assert(key::Space == VK_SPACE, "key id Space must equal VK_SPACE");
static_assert(key::CapsLock == VK_CAPITAL, "key id CapsLock must equal VK_CAPITAL");
static_assert(key::LShift == VK_LSHIFT, "key id LShift must equal VK_LSHIFT");
static_assert(key::RShift == VK_RSHIFT, "key id RShift must equal VK_RSHIFT");
static_assert(key::LControl == VK_LCONTROL, "key id LControl must equal VK_LCONTROL");
static_assert(key::RControl == VK_RCONTROL, "key id RControl must equal VK_RCONTROL");
static_assert(key::LAlt == VK_LMENU, "key id LAlt must equal VK_LMENU");
static_assert(key::RAlt == VK_RMENU, "key id RAlt must equal VK_RMENU");
static_assert(key::LWin == VK_LWIN, "key id LWin must equal VK_LWIN");
static_assert(key::RWin == VK_RWIN, "key id RWin must equal VK_RWIN");
static_assert(key::Apps == VK_APPS, "key id Apps must equal VK_APPS");
static_assert(key::PageUp == VK_PRIOR, "key id PageUp must equal VK_PRIOR");
static_assert(key::PageDown == VK_NEXT, "key id PageDown must equal VK_NEXT");
static_assert(key::Home == VK_HOME, "key id Home must equal VK_HOME");
static_assert(key::End == VK_END, "key id End must equal VK_END");
static_assert(key::Insert == VK_INSERT, "key id Insert must equal VK_INSERT");
static_assert(key::Delete == VK_DELETE, "key id Delete must equal VK_DELETE");
static_assert(key::Left == VK_LEFT, "key id Left must equal VK_LEFT");
static_assert(key::Right == VK_RIGHT, "key id Right must equal VK_RIGHT");
static_assert(key::Up == VK_UP, "key id Up must equal VK_UP");
static_assert(key::Down == VK_DOWN, "key id Down must equal VK_DOWN");
static_assert(key::PrintScreen == VK_SNAPSHOT, "key id PrintScreen must equal VK_SNAPSHOT");
static_assert(key::ScrollLock == VK_SCROLL, "key id ScrollLock must equal VK_SCROLL");
static_assert(key::Pause == VK_PAUSE, "key id Pause must equal VK_PAUSE");
static_assert(key::NumLock == VK_NUMLOCK, "key id NumLock must equal VK_NUMLOCK");
static_assert(key::Semicolon == VK_OEM_1, "key id Semicolon must equal VK_OEM_1");
static_assert(key::Equals == VK_OEM_PLUS, "key id Equals must equal VK_OEM_PLUS");
static_assert(key::Comma == VK_OEM_COMMA, "key id Comma must equal VK_OEM_COMMA");
static_assert(key::Minus == VK_OEM_MINUS, "key id Minus must equal VK_OEM_MINUS");
static_assert(key::Period == VK_OEM_PERIOD, "key id Period must equal VK_OEM_PERIOD");
static_assert(key::Slash == VK_OEM_2, "key id Slash must equal VK_OEM_2");
static_assert(key::Backquote == VK_OEM_3, "key id Backquote must equal VK_OEM_3");
static_assert(key::LeftBracket == VK_OEM_4, "key id LeftBracket must equal VK_OEM_4");
static_assert(key::Backslash == VK_OEM_5, "key id Backslash must equal VK_OEM_5");
static_assert(key::RightBracket == VK_OEM_6, "key id RightBracket must equal VK_OEM_6");
static_assert(key::Quote == VK_OEM_7, "key id Quote must equal VK_OEM_7");
static_assert(key::F1 == VK_F1, "key id F1 must equal VK_F1");
static_assert(key::F4 == VK_F4, "key id F4 must equal VK_F4");
static_assert(key::VolumeUp == VK_VOLUME_UP, "key id VolumeUp must equal VK_VOLUME_UP");
static_assert(key::VolumeDown == VK_VOLUME_DOWN, "key id VolumeDown must equal VK_VOLUME_DOWN");
static_assert(key::VolumeMute == VK_VOLUME_MUTE, "key id VolumeMute must equal VK_VOLUME_MUTE");
static_assert(key::MediaPlayPause == VK_MEDIA_PLAY_PAUSE, "key id MediaPlayPause must equal VK_MEDIA_PLAY_PAUSE");
static_assert(key::MediaNext == VK_MEDIA_NEXT_TRACK, "key id MediaNext must equal VK_MEDIA_NEXT_TRACK");
static_assert(key::MediaPrevious == VK_MEDIA_PREV_TRACK, "key id MediaPrevious must equal VK_MEDIA_PREV_TRACK");
static_assert(key::MediaStop == VK_MEDIA_STOP, "key id MediaStop must equal VK_MEDIA_STOP");
static_assert(key::BrowserBack == VK_BROWSER_BACK, "key id BrowserBack must equal VK_BROWSER_BACK");
static_assert(key::BrowserForward == VK_BROWSER_FORWARD, "key id BrowserForward must equal VK_BROWSER_FORWARD");
static_assert(key::BrowserRefresh == VK_BROWSER_REFRESH, "key id BrowserRefresh must equal VK_BROWSER_REFRESH");
static_assert(key::BrowserHome == VK_BROWSER_HOME, "key id BrowserHome must equal VK_BROWSER_HOME");
static_assert(key::LaunchMail == VK_LAUNCH_MAIL, "key id LaunchMail must equal VK_LAUNCH_MAIL");
static_assert(key::Ctrl == VK_CONTROL, "key id Ctrl must equal VK_CONTROL");
static_assert(key::Alt == VK_MENU, "key id Alt must equal VK_MENU");
static_assert(key::Shift == VK_SHIFT, "key id Shift must equal VK_SHIFT");

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

Engine* g_engine = nullptr;  // the platform-independent decisions; this file only reports events and injects keys
HHOOK g_hook = nullptr;
HWND g_rawWindow = nullptr;
DWORD g_mainThread = 0;

std::mutex g_lastMutex;
std::string g_lastDevice;

// Hook-thread-only state.
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
        case VK_BROWSER_BACK: case VK_BROWSER_FORWARD: case VK_BROWSER_REFRESH: case VK_BROWSER_HOME:
        case VK_VOLUME_MUTE: case VK_VOLUME_DOWN: case VK_VOLUME_UP:
        case VK_MEDIA_NEXT_TRACK: case VK_MEDIA_PREV_TRACK: case VK_MEDIA_STOP: case VK_MEDIA_PLAY_PAUSE:
        case VK_LAUNCH_MAIL:
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

    const auto vk = static_cast<unsigned short>(kb->vkCode);
    const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);

    std::vector<KeyEvent> send;
    const bool swallow = g_engine->handle(attribute(*kb, up), vk, up, send);
    for (const KeyEvent& e : send) sendKey(e.key, e.up);
    return swallow ? 1 : CallNextHookEx(g_hook, code, wParam, lParam);
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

namespace platform {

std::string exeDir() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf, n);
    return path.substr(0, path.find_last_of("\\/"));
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

int runHook(Engine& engine) {
    g_engine = &engine;
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

}  // namespace platform

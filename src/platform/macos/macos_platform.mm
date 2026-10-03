#include "platform.h"

#include "engine.h"
#include "mac_input.h"
#include "matcher.h"

#import <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDLib.h>  // IOHIDRequestAccess
#include <crt_externs.h>  // _NSGetEnviron
#include <mach-o/dyld.h>

#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

// The decisions (key tables, modifier flags, which keyboard) are in mac_input, which is tested; this
// file connects them to the event tap, IOHIDManager and CGEventPost.
static_assert(mac::kFlagShift == kCGEventFlagMaskShift && mac::kFlagControl == kCGEventFlagMaskControl &&
                  mac::kFlagAlternate == kCGEventFlagMaskAlternate && mac::kFlagCommand == kCGEventFlagMaskCommand &&
                  mac::kFlagAlphaShift == kCGEventFlagMaskAlphaShift &&
                  mac::kFlagNumericPad == kCGEventFlagMaskNumericPad &&
                  mac::kFlagSecondaryFn == kCGEventFlagMaskSecondaryFn,
              "mac_input.h flag values must equal CGEventFlags");

namespace {

// Tag on events we inject (kCGEventSourceUserData), so the tap never remaps its own output.
constexpr int64_t kInjectedTag = 0x4B4D4150;  // 'KMAP'

Engine* g_engine = nullptr;  // the platform-independent decisions; this file only reports events and injects keys
CFMachPortRef g_tap = nullptr;
CGEventSourceRef g_source = nullptr;
std::atomic<CFRunLoopRef> g_runLoop{nullptr};
std::atomic<bool> g_quitRequested{false};

std::mutex g_hidMutex;  // guards g_attributor: the HID thread fills it, the tap thread reads it
mac::Attributor g_attributor;
mac::ModifierState g_mods;  // tap thread only

std::mutex g_hidutilMutex;           // guards g_hidMappings: configs arrive from the server thread too
std::vector<mac::HidMapping> g_hidMappings;  // what we last set with hidutil

// Sets the HID system's key mapping for every keyboard. Kept until changed, or until the Mac restarts.
void setHidMappings(const std::vector<mac::HidMapping>& mappings) {
    std::lock_guard<std::mutex> lock(g_hidutilMutex);
    if (mappings == g_hidMappings) return;  // never touch a mapping set by hand unless we set one
    const std::string json = mac::userKeyMappingJson(mappings);
    const char* argv[] = {"/usr/bin/hidutil", "property", "--set", json.c_str(), nullptr};
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);  // it echoes the mapping
    pid_t pid = 0;
    int status = 0;
    const bool ok = posix_spawn(&pid, argv[0], &actions, nullptr, const_cast<char* const*>(argv), *_NSGetEnviron()) == 0 &&
                    waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    posix_spawn_file_actions_destroy(&actions);
    if (!ok) {
        std::cerr << "hidutil could not set the Caps Lock mappings.\n";
        return;
    }
    if (g_hidMappings.empty()) {
        static bool registered = false;  // on any exit, Caps Lock and the keys it was swapped with act normally again
        if (!registered) registered = std::atexit([] { setHidMappings({}); }) == 0;
    }
    g_hidMappings = mappings;
}

int numberProperty(IOHIDDeviceRef device, CFStringRef key) {
    int value = 0;
    auto n = static_cast<CFNumberRef>(IOHIDDeviceGetProperty(device, key));
    if (n && CFGetTypeID(n) == CFNumberGetTypeID()) CFNumberGetValue(n, kCFNumberIntType, &value);
    return value;
}

std::string deviceId(IOHIDDeviceRef device) {
    return mac::deviceId(numberProperty(device, CFSTR(kIOHIDVendorIDKey)),
                         numberProperty(device, CFSTR(kIOHIDProductIDKey)));
}

std::string productName(IOHIDDeviceRef device) {
    auto s = static_cast<CFStringRef>(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey)));
    if (!s || CFGetTypeID(s) != CFStringGetTypeID()) return "";
    char buf[256];
    if (!CFStringGetCString(s, buf, sizeof buf, kCFStringEncodingUTF8)) return "";
    return buf;
}

IOHIDManagerRef createKeyboardManager() {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    int page = kHIDPage_GenericDesktop, usage = kHIDUsage_GD_Keyboard;
    CFNumberRef pageNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
    CFNumberRef usageNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    const void* keys[] = {CFSTR(kIOHIDDeviceUsagePageKey), CFSTR(kIOHIDDeviceUsageKey)};
    const void* values[] = {pageNum, usageNum};
    CFDictionaryRef match = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
                                               &kCFTypeDictionaryValueCallBacks);
    IOHIDManagerSetDeviceMatching(manager, match);
    CFRelease(match);
    CFRelease(usageNum);
    CFRelease(pageNum);
    return manager;
}

void hidValueCallback(void*, IOReturn, void*, IOHIDValueRef value) {
    IOHIDElementRef element = IOHIDValueGetElement(value);
    if (IOHIDElementGetUsagePage(element) != kHIDPage_KeyboardOrKeypad) return;
    const auto k = mac::keyFromHid(IOHIDElementGetUsage(element));
    if (!k) return;
    const std::string device = deviceId(IOHIDElementGetDevice(element));
    std::lock_guard<std::mutex> lock(g_hidMutex);
    g_attributor.hid(*k, IOHIDValueGetIntegerValue(value) == 0, device, std::chrono::steady_clock::now());
}

// Watches every keyboard on its own thread, so HID reports are queued while the tap callback runs.
void startHidThread() {
    std::thread([] {
        IOHIDManagerRef manager = createKeyboardManager();
        IOHIDManagerRegisterInputValueCallback(manager, hidValueCallback, nullptr);
        IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        const IOReturn r = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
        if (r != kIOReturnSuccess) {
            std::cerr << "Cannot watch keyboards (" << std::hex << r << std::dec << "): allow this program under "
                      << "System Settings > Privacy & Security > Input Monitoring. Using the default config for "
                      << "every keyboard.\n";
            return;
        }
        CFRunLoopRun();
    }).detach();
}

void sendMediaKey(int nx, bool up) {
    @autoreleasepool {
        const long data1 = mac::encodeMedia(nx, up);
        NSEvent* ev = [NSEvent otherEventWithType:NSEventTypeSystemDefined
                                         location:NSZeroPoint
                                    modifierFlags:static_cast<NSEventModifierFlags>(data1 & 0xFF00)
                                        timestamp:0
                                     windowNumber:0
                                          context:nil
                                          subtype:mac::kAuxControlButtons
                                            data1:data1
                                            data2:-1];
        CGEventRef cg = [ev CGEvent];
        if (!cg) return;
        CGEventSetIntegerValueField(cg, kCGEventSourceUserData, kInjectedTag);
        CGEventPost(kCGHIDEventTap, cg);
    }
}

void sendKey(unsigned short k, bool up) {
    if (const auto nx = mac::mediaFromKey(k)) return sendMediaKey(*nx, up);
    const auto code = mac::macFromKey(k);
    if (!code) return;  // no such key on macOS
    CGEventRef ev = CGEventCreateKeyboardEvent(g_source, *code, !up);
    if (!ev) return;
    if (mac::modifierFlags(*code)) {
        g_mods.injected(*code, up);
        CGEventSetType(ev, kCGEventFlagsChanged);  // what a real modifier key produces
    }
    // Keep Caps Lock as it is; the modifiers are the ones held now.
    const CGEventFlags capsLock = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState) & kCGEventFlagMaskAlphaShift;
    CGEventSetFlags(ev, g_mods.apply(capsLock | mac::keyFlags(*code)));
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

// Report one physical event to the engine and inject what it asks for. Returns true to swallow it.
bool report(unsigned short k, bool up) {
    std::string device;
    {
        std::lock_guard<std::mutex> lock(g_hidMutex);
        device = g_attributor.attribute(k, up, std::chrono::steady_clock::now());
    }
    std::vector<KeyEvent> send;
    const bool swallow = g_engine->handle(device, k, up, send);
    for (const KeyEvent& e : send) sendKey(e.key, e.up);
    return swallow;
}

CGEventRef pass(CGEventRef ev) {
    CGEventSetFlags(ev, g_mods.passed(CGEventGetFlags(ev)));
    return ev;
}

CGEventRef tapCallback(CGEventTapProxy, CGEventType type, CGEventRef ev, void*) {
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (g_tap) CGEventTapEnable(g_tap, true);  // macOS turns a slow tap off; turn it back on
        return ev;
    }
    if (CGEventGetIntegerValueField(ev, kCGEventSourceUserData) == kInjectedTag) return ev;

    if (type == static_cast<CGEventType>(mac::kSystemDefined)) {
        @autoreleasepool {
            NSEvent* ns = [NSEvent eventWithCGEvent:ev];
            if (ns.subtype != mac::kAuxControlButtons) return ev;
            const mac::MediaKey m = mac::decodeMedia(ns.data1);
            if (const auto k = mac::keyFromMedia(m.nx)) return report(*k, m.up) ? nullptr : ev;
        }
        return ev;
    }

    const auto code = static_cast<uint16_t>(CGEventGetIntegerValueField(ev, kCGKeyboardEventKeycode));
    const auto k = mac::keyFromMac(code);
    if (type == kCGEventFlagsChanged) {
        // Caps Lock and Fn also come through here. macOS toggles Caps Lock itself and reports no
        // release, so neither is remapped.
        if (!k || !mac::modifierFlags(code)) return ev;
        const bool up = mac::isModifierRelease(code, CGEventGetFlags(ev));
        const bool swallow = report(*k, up);
        g_mods.physical(code, up, swallow);
        return swallow ? nullptr : pass(ev);
    }

    // kCGEventKeyDown / kCGEventKeyUp
    if (k && report(*k, type == kCGEventKeyUp)) return nullptr;
    return pass(ev);
}

}  // namespace

namespace platform {

std::string exeDir() {
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) return ".";
    path.resize(std::strlen(path.c_str()));
    std::error_code ec;
    const auto real = std::filesystem::canonical(path, ec);  // resolve symlinks and ./..
    return (ec ? std::filesystem::path(path) : real).parent_path().string();
}

std::vector<KeyboardInfo> listKeyboards() {
    std::vector<KeyboardInfo> out;
    IOHIDManagerRef manager = createKeyboardManager();
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (devices) {
        std::vector<const void*> list(CFSetGetCount(devices));
        CFSetGetValues(devices, list.data());
        for (const void* d : list) {
            auto device = static_cast<IOHIDDeviceRef>(const_cast<void*>(d));
            const std::string id = deviceId(device);
            if (std::any_of(out.begin(), out.end(), [&](const KeyboardInfo& k) { return k.id == id; })) continue;
            const std::string name = productName(device);
            out.push_back({id, name.empty() ? id : name});
        }
        CFRelease(devices);
    }
    CFRelease(manager);
    return out;
}

std::string lastKeyboard() {
    std::lock_guard<std::mutex> lock(g_hidMutex);
    return g_attributor.last();
}

const char* name() { return "macos"; }

bool openUrl(const std::string& url) {
    @autoreleasepool {
        NSURL* u = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
        return u && [[NSWorkspace sharedWorkspace] openURL:u];
    }
}

void adoptConfig(const std::string& deviceId, Config& cfg) {
    auto mappings = mac::takeCapsLockMappings(cfg.base.map);
    if (deviceId.empty()) return setHidMappings(mappings);
    if (!mappings.empty())  // the HID system's mapping is the same for every keyboard
        std::cerr << "Keyboard config " << deviceId << ": Caps Lock mappings only work in the default config.\n";
}

void requestQuit() {
    g_quitRequested = true;
    if (CFRunLoopRef loop = g_runLoop.load()) CFRunLoopStop(loop);
}

// One copy per user. A lock file in the user's temp folder says "a copy is running" and holds its
// process id; a newer copy sends it SIGTERM and waits for the lock to be released.
bool takeOverFromRunningInstance() {
    // Ctrl+C and SIGTERM (from a newer copy, or `kill`) stop the hook cleanly.
    for (int sig : {SIGINT, SIGTERM}) {
        signal(sig, SIG_IGN);
        dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, sig, 0,
                                                       dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0));
        dispatch_source_set_event_handler(src, ^{ requestQuit(); });
        dispatch_resume(src);
    }

    const std::string path = (std::filesystem::temp_directory_path() / "keymapper.lock").string();
    const int fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true;  // can't tell: carry on without the protection

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        std::cout << "Keymapper is already running: stopping it and taking over...\n";
        char buf[32] = {};
        if (pread(fd, buf, sizeof buf - 1, 0) > 0) {
            const pid_t pid = static_cast<pid_t>(std::atoi(buf));
            if (pid > 0) kill(pid, SIGTERM);
        }
        bool locked = false;
        for (int i = 0; i < 80 && !locked; ++i) {  // released when the old copy exits
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            locked = flock(fd, LOCK_EX | LOCK_NB) == 0;
        }
        if (!locked) {
            std::cerr << "The running copy did not stop in time.\n";
            close(fd);
            return false;
        }
    }
    // Held until this process exits (the descriptor is never closed).
    const std::string pid = std::to_string(getpid());
    if (ftruncate(fd, 0) == 0) pwrite(fd, pid.data(), pid.size(), 0);
    return true;
}

int runHook(Engine& engine) {
    g_engine = &engine;

    // Prompts for the two permissions the first time (System Settings > Privacy & Security).
    IOHIDRequestAccess(kIOHIDRequestTypeListenEvent);
    @autoreleasepool {
        AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef) @{(__bridge id)kAXTrustedCheckOptionPrompt : @YES});
    }
    startHidThread();

    g_source = CGEventSourceCreate(kCGEventSourceStatePrivate);
    if (g_source) CGEventSourceSetUserData(g_source, kInjectedTag);

    const CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                             CGEventMaskBit(kCGEventFlagsChanged) | CGEventMaskBit(mac::kSystemDefined);
    g_tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask, tapCallback,
                             nullptr);
    if (!g_tap) {
        std::cerr << "Cannot intercept keys: allow this program (or the terminal that runs it) under "
                  << "System Settings > Privacy & Security > Accessibility, then start it again.\n";
        return 1;
    }
    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CGEventTapEnable(g_tap, true);

    g_runLoop = CFRunLoopGetCurrent();
    while (!g_quitRequested) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);  // also catches an early quit
    g_runLoop = nullptr;

    CGEventTapEnable(g_tap, false);
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CFRelease(src);
    CFRelease(g_tap);
    g_tap = nullptr;
    return 0;
}

}  // namespace platform

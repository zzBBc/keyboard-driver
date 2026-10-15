#include "input_source.h"

#include <Carbon/Carbon.h>

namespace mac {

namespace {

std::string sourceString(TISInputSourceRef s, CFStringRef property) {
    const auto value = static_cast<CFStringRef>(TISGetInputSourceProperty(s, property));
    char buf[256];
    return value && CFStringGetCString(value, buf, sizeof buf, kCFStringEncodingUTF8) ? buf : "";
}

bool sourceFlag(TISInputSourceRef s, CFStringRef property) {
    const auto value = static_cast<CFBooleanRef>(TISGetInputSourceProperty(s, property));
    return value && CFBooleanGetValue(value);
}

}  // namespace

// macOS ignores a posted Fn press for the Globe key's Change Input Source, so it is done here.
void selectNextInputSource() {
    CFArrayRef list = TISCreateInputSourceList(nullptr, false);  // the enabled sources
    if (!list) return;
    std::vector<InputSource> enabled;
    for (CFIndex i = 0; i < CFArrayGetCount(list); ++i) {
        const auto s = (TISInputSourceRef)(CFArrayGetValueAtIndex(list, i));
        const auto category = static_cast<CFStringRef>(TISGetInputSourceProperty(s, kTISPropertyInputSourceCategory));
        enabled.push_back({sourceString(s, kTISPropertyInputSourceID),
                           category && CFEqual(category, kTISCategoryKeyboardInputSource),
                           sourceFlag(s, kTISPropertyInputSourceIsSelectCapable)});
    }
    std::string current;
    if (TISInputSourceRef cur = TISCopyCurrentKeyboardInputSource()) {
        current = sourceString(cur, kTISPropertyInputSourceID);
        CFRelease(cur);
    }
    if (const auto next = nextInputSource(enabled, current))
        for (CFIndex i = 0; i < CFArrayGetCount(list); ++i) {
            const auto s = (TISInputSourceRef)(CFArrayGetValueAtIndex(list, i));
            if (sourceString(s, kTISPropertyInputSourceID) == *next) { TISSelectInputSource(s); break; }
        }
    CFRelease(list);
}

}  // namespace mac

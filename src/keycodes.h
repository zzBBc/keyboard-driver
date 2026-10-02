#pragma once

// Portable key ids used everywhere outside the platform layer (config, matcher, engine, tests).
//
// The numeric values equal the Windows virtual-key codes, so the Windows layer passes them straight
// to the OS (windows_platform.cpp static_asserts the ones it relies on). Other platforms translate
// their native key codes to and from these ids.
namespace key {

enum : unsigned short {
    Backspace = 0x08,
    Tab = 0x09,
    Enter = 0x0D,
    Shift = 0x10,   // either side
    Ctrl = 0x11,    // either side
    Alt = 0x12,     // either side
    Pause = 0x13,
    CapsLock = 0x14,
    Escape = 0x1B,
    Space = 0x20,
    PageUp = 0x21,
    PageDown = 0x22,
    End = 0x23,
    Home = 0x24,
    Left = 0x25,
    Up = 0x26,
    Right = 0x27,
    Down = 0x28,
    PrintScreen = 0x2C,
    Insert = 0x2D,
    Delete = 0x2E,
    LWin = 0x5B,
    RWin = 0x5C,
    Apps = 0x5D,
    F1 = 0x70,  // F1..F24 are contiguous: F1 + n - 1
    F4 = 0x73,
    F24 = 0x87,
    NumLock = 0x90,
    ScrollLock = 0x91,
    LShift = 0xA0,
    RShift = 0xA1,
    LControl = 0xA2,
    RControl = 0xA3,
    LAlt = 0xA4,
    RAlt = 0xA5,
    BrowserBack = 0xA6,
    BrowserForward = 0xA7,
    BrowserRefresh = 0xA8,
    BrowserHome = 0xAC,
    VolumeMute = 0xAD,
    VolumeDown = 0xAE,
    VolumeUp = 0xAF,
    MediaNext = 0xB0,
    MediaPrevious = 0xB1,
    MediaStop = 0xB2,
    MediaPlayPause = 0xB3,
    LaunchMail = 0xB4,
    Semicolon = 0xBA,
    Equals = 0xBB,
    Comma = 0xBC,
    Minus = 0xBD,
    Period = 0xBE,
    Slash = 0xBF,
    Backquote = 0xC0,
    LeftBracket = 0xDB,
    Backslash = 0xDC,
    RightBracket = 0xDD,
    Quote = 0xDE,
};

}  // namespace key

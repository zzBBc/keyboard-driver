# Keymapper for Android tablets

The same program as on Windows and macOS: `src/core` and `src/server` are compiled by CMake into
`libkeymapper.so` (`src/platform/android/`), and this Kotlin app hosts it. `web/`, `config/actions.android.txt`
and `config/mappings.txt` are packaged as assets by Gradle, so there is no second copy of the GUI.

- `KeymapperService`: an accessibility service. Android hands it every hardware-keyboard event before the
  app in front, and lets it swallow them. Each event goes to the engine through `NativeBridge.onKey`.
- `MainActivity`: shows the GUI (a WebView on `http://127.0.0.1:8765`), or a button to the accessibility
  settings while the service is off.

## What it can do

Android does not let an app inject key presses without root, so the engine's output is limited to the
system's global actions: Back, Home, Recents, Notifications, Quick settings, Power menu, Lock screen and
Screenshot (`config/actions.android.txt`). Layers, combos and `none` work as on the other platforms. A
mapping that would send any other key (`h = left`, `a = b`) is not applied: the original key goes through.
Keys such as `caps = none` (swallowed, nothing sent) work.

Keyboards are told apart by USB `VID_xxxx&PID_xxxx`, like on Windows.

## Build

Needs the Android SDK (platform 34), NDK 26 with CMake 3.22 and Gradle 8.7 on JDK 17:

```
gradle -p android assembleDebug
```

The APK is `android/app/build/outputs/apk/debug/app-debug.apk`, signed with the debug key. The release
workflows publish it as `release/android/keymapper-android.apk`.

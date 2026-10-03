# Keymapper

A key remapper for Windows and macOS with a browser GUI. Remap keys, define layers (hold a key to change what
other keys do), bind key combos to ready-made actions, and give each physical keyboard its own config.

## Build

Requires CMake 3.16+ and a C++17 compiler: MSVC on Windows, Xcode or its Command Line Tools on macOS.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build copies `web/`, the built-in actions (`actions.windows.txt`, `actions.macos.txt`) and the default config
(`mappings.default.txt`) next to the program: `build\Release\` on Windows, `build/` on macOS.

## Run

```
build\Release\keymapper.exe [config-path] [port]     # Windows
build/keymapper [config-path] [port]                  # macOS
```

It opens http://127.0.0.1:8765 (the default port) in your default browser. A global keyboard hook is active while it runs,
so remaps apply to every program.

To stop it, click **Stop app** in the GUI's bottom bar (or press Ctrl+C in its console window, or
`taskkill /IM keymapper.exe`). Only one copy runs at a time: starting `keymapper.exe` again stops the
running copy and takes over its port and hook.

The GUI server only listens on localhost and rejects requests from other sites.

### macOS

To run the release zip:

1. Unzip it and move the folder out of Downloads, for example to `~/Applications/keymapper`. macOS
   ties the permissions below to the program's path, so keep it in one place.
2. The zip is not signed, so macOS says it "could not verify keymapper is free of malware". Clear the
   download quarantine from the whole folder:

   ```
   xattr -dr com.apple.quarantine ~/Applications/keymapper
   ```

   If `xattr` says "Operation not permitted", your terminal app may not have access to that folder:
   grant it under **System Settings > Privacy & Security > Files and Folders** (or **Full Disk
   Access**), or run the command from another terminal. Instead of `xattr`, you can also start
   keymapper once, then click **Open Anyway** in **System Settings > Privacy & Security**.
3. Start it (`./keymapper` in the folder). On first start macOS asks for two permissions. Turn both on
   in **System Settings > Privacy & Security**:
   - **Accessibility**: to intercept and send keys. Remapping does nothing without it.
   - **Input Monitoring**: to tell keyboards apart. Without it every keyboard uses the default config.

   If the app is not in a list, click **+** and add it. When you start keymapper from a terminal,
   the permissions belong to the terminal app (Terminal, iTerm, ...), so turn them on for that app.
4. Restart keymapper after granting the permissions.

A new release replaces the program, and macOS may ask for the permissions again. If remapping stops
working after an update, remove the old entry with **-** in both lists and grant it again.

- Stop it with **Stop app**, Ctrl+C, or `pkill keymapper`. Starting it again stops the running copy,
  as on Windows.
- Combos can use the Mac modifier names: `cmd` (or `command`), `option` (or `opt`), `control`. They
  mean the same as `win`, `alt`, `ctrl`, which also work, and the GUI writes the Mac names. Key
  names keep their Windows names: `lwin`/`rwin` is Command, `lalt`/`ralt` is Option, `apps` is the
  context-menu key, `insert` is Help.
- The built-in actions come from `config/actions.macos.txt` and send Mac shortcuts (`cmd+w`,
  `control+up`...) under the same names as on Windows, so a config that binds `@close-window`
  works on both.
- The GUI uses the Mac names (Control, Option, Cmd, Help, Delete) and a Mac keyboard layout, and
  leaves out the keys macOS has no code for. The config file still uses the names above.

## Using the GUI

- **Keyboard** dropdown: choose which keyboard you are editing, or the default config.
  **Detect** selects the keyboard you typed on last.
- **Base** tab: mappings that are always active. A **Layer** tab applies only while its key is held.
- **Keyboard picture**: click a key, then pick what it becomes from the list (actions and keys), or
  use **Pick on keyboard** and click the key. It includes F13-F24 and the media and browser keys
  (on macOS: F13-F20 and the media keys). Tick Ctrl/Alt/Shift/Win (Control/Option/Shift/Cmd on
  macOS) first to bind a combo such as Alt+Q. A combo or steps written by hand in
  the config show as "Custom" in the list and are kept.
- **Actions** tab: every built-in action, grouped by category, with its description and the keys it
  sends. Click **+ Shortcut** on one and press the keys (or tick modifiers and pick a key) to bind it;
  the keys the OS keeps for itself, like Alt+Tab (Cmd+Tab on macOS), can't be recorded in a browser, so use the boxes.
  **Remove shortcut** (or a chip's ×) removes only the combo; the action stays. **Change keys** changes
  what an action sends (for example Undo sends `alt+z` instead of `ctrl+z`); **Reset keys** undoes that.
- **Save** applies the change immediately, no restart needed.

## Config files

| File | Used by |
|---|---|
| `mappings.txt` (next to the exe) | the default: every keyboard without its own file |
| `devices\<hardware id>.txt` | one specific keyboard, e.g. `devices\VID_048D&PID_C108.txt` |
| `actions.windows.txt` / `actions.macos.txt` (next to the exe) | the built-in actions for each OS (the program loads the one for the OS it runs on), copied from `config/` on every build; do not edit these copies |

On first run `mappings.txt` is created from `mappings.default.txt` and never overwritten. Configs are
edited by the GUI, or by hand (restart to pick up hand edits).

Syntax:

```
# base mappings, always active
tab = capslock
ralt = rctrl

# while CapsLock is held
[layer CapsLock]
h = left
j = down
k = up
l = right

[base]   # switch back to base mappings
```

### Combos and actions

`from` can be a combo (modifiers `ctrl`, `alt`, `shift`, `win`, or the Mac names `control`, `option`,
`cmd`, plus one key) and `to` can be a key, a
combo, comma-separated steps, or `@action`. An action is a named list of steps, defined once and
bound anywhere:

```
alt+q = @switch-window       # Alt+Q runs the action
alt+w = ctrl+shift+tab       # or a combo directly
alt+e = none                 # clears a binding (see below)

[action switch-window]
alt+tab

[action copy-paste]
ctrl+c, ctrl+v               # steps run in order
```

- A combo fires only when exactly those modifiers are held, and the trigger key is swallowed.
- If a step wants a modifier you are already holding (Alt for `alt+tab`), it is left held, so the
  Windows switcher stays open while you keep Alt down. Other held modifiers are released for the
  step and put back afterwards.
- Actions work in base, layers and per-keyboard files. A `from = to` line after an action block
  ends that block.
- An action can start with `description: text`. Library actions also have `category: Name` (groups
  them in the GUI, in the order the file lists them) and optional `shortcut: alt+q` lines.
- `alt+q = none` removes the binding for that combo, including a default shortcut declared by a
  library action (a default is bound in Base unless your config binds or clears that combo). It does
  not block the key itself. In the GUI a default shows as a chip marked "(default)" and its ×
  writes the `none` line. The shipped action lists declare no defaults at the moment.
- Defining an action with the same name as a built-in one in your config overrides it.

**Built-in actions** (`config/actions.windows.txt` on Windows) are 49 common shortcuts in five categories:
- *Windows and desktops:* `switch-window`, `previous-window`, `task-view`, `close-window`, `quit-app`,
  `show-desktop`, `lock-screen`, `snap-left`, `next-desktop`, ...
- *Media and browser:* `volume-up`, `volume-down`, `mute`, `play-pause`, `next-track`,
  `previous-track`, `stop-media`, `browser-back`/`-forward`/`-refresh`/`-home`, `launch-mail`.
- *Launch:* `file-explorer`, `run-dialog`, `screenshot-screen`, `screenshot-region` (each also as
  `-clipboard`), `screenshot-tool`, `emoji-picker`, `clipboard-history`.
- *Editing:* `copy`, `cut`, `paste`, `undo`, `redo`, `select-all`, ...
- *Tabs:* `next-tab`, `previous-tab`, `new-tab`, `close-tab`, `reopen-tab`.

On macOS the list is `config/actions.macos.txt` (45 actions): the same names with Mac shortcuts
(`task-view` opens Mission Control, `run-dialog` opens Spotlight, `copy` sends `cmd+c`...), plus
`next-app-window` and `hide-app`, which Windows has no shortcut for. Actions macOS has no shortcut
for (`snap-left`/`-right`, `new-desktop`, `clipboard-history`, `stop-media`, `launch-mail`) are left
out: window tiling needs the Fn (Globe) key, which keymapper can't send, and macOS has no stop or
mail key.

None has a default shortcut. The GUI chooses actions from these lists and does not create new ones;
to define your own, write an `[action ...]` block in the config file. The media actions send the same
keys as a keyboard's media keys, so they can put a media function on a combo of your choice (for
example `alt+up = @volume-up`).

### Key names

Lowercase: letters, digits, `f1`-`f24`, `esc`, `tab`, `capslock`, `space`, `enter`, `backspace`,
`lshift`/`rshift`, `lctrl`/`rctrl`, `lalt`/`ralt`, `lwin`/`rwin`, arrows, `home`/`end`/`pageup`/
`pagedown`/`insert`/`delete`, `printscreen`/`scrolllock`/`pause`/`numlock`/`apps`, punctuation
(`;` `=` `,` `-` `.` `/` `[` `\` `]` `'` and the backquote key), and the media keys `volumeup`, `volumedown`, `mute`, `playpause`,
`nexttrack`, `previoustrack`, `mediastop`, `browserback`, `browserforward`, `browserrefresh`,
`browserhome`, `launchmail`. The GUI lists them all.

## Per-keyboard configs

Keyboards are told apart by hardware ID (USB `VID_xxxx&PID_xxxx`, or the bus ID for built-in
keyboards such as `FUJ7401`). The low-level hook can't say which keyboard sent a key, so Raw Input
events are matched to hook events by scan code and timing.

## Limitations

- Two identical keyboards (same VID/PID) share one config.
- If two keyboards press the same key at nearly the same instant, one may be misattributed.
- The Fn key is handled inside most keyboards and never reaches Windows, so it can't be remapped.
  The keys an Fn combo produces (volume, brightness...) can be, if Windows sees them as keys.
  Screen brightness has no key Windows accepts, so there is no brightness action.
- Like any user-level hook, it does not see keys typed into windows running as administrator, UAC
  prompts or the login screen.
- No numpad key names yet. Names follow Windows virtual-key codes, so punctuation names refer to the
  key positions of a US layout.
- macOS: macOS toggles Caps Lock before any program sees the key, so plain mappings to or from
  Caps Lock (`capslock = tab`, `tab = capslock`) are handed to the HID system with `hidutil`
  instead. They apply to every keyboard, only work in the default config, and replace any mapping
  you set with `hidutil` yourself; quitting keymapper clears them (after a crash, run
  `hidutil property --set '{"UserKeyMapping":[]}'` or restart). Caps Lock can't be a layer key,
  and Fn can't be remapped. PrintScreen, ScrollLock, Pause, F21-F24, `mediastop` and the
  browser and mail keys have no macOS key, so they are never seen and sending them does nothing.
- There is no Linux platform layer yet.

## Tests

```
cmake --build build --config Release --target keymapper_tests
build\Release\keymapper_tests.exe     # Windows; build/keymapper_tests on macOS
```

The GUI's logic (config parsing, chords, actions, shortcuts) has its own tests, which need Node.js:

```
node --test "tests/web/*.test.js"
```

## Layout

The program is a portable core plus one small layer per operating system:

- `src/core/`: no OS calls. Config parsing (`config_loader`), key names and portable key ids
  (`key_names`, `keycodes.h`; the ids equal Windows virtual-key codes, other OSes translate to and
  from them), chord matching (`matcher`), the **engine** (`engine`: given each physical key event
  and the keyboard it came from, decides whether to swallow it and which keys to inject), and the
  rules for which GUI files may be served (`static_files`). Built as the `keymapper_core` library.
- `src/server/server.cpp`: localhost HTTP server and API (`/api/config`, `/api/devices`,
  `/api/keys`, `/api/actions`, `/api/platform`) plus the GUI's scripts and styles from `web/`. It uses
  `std::filesystem` and a small Winsock/BSD sockets shim, so it is portable too.
- `src/platform/platform.h`: what the program needs from the OS (find the install folder, list
  keyboards, intercept keys and inject the engine's output).
  `src/platform/windows/windows_platform.cpp` implements it with a low-level keyboard hook, Raw
  Input (to tell keyboards apart) and `SendInput`. `src/platform/macos/macos_platform.mm` implements
  it with a Quartz event tap, IOHIDManager (to tell keyboards apart) and `CGEventPost`, and AppKit
  for the media keys and opening the browser. Its decisions (macOS key codes to portable ids,
  modifier flags, media key events, which keyboard a key came from) are in `mac_input.cpp`, which
  uses no Apple API, so `tests/macos/` can test it without a keyboard tap.
- `src/main.cpp`: loads configs, creates the engine, starts the server and the platform hook.
- `config/`: `actions.windows.txt` and `actions.macos.txt` (the built-in actions for each OS)
  and `mappings.txt` (the default config).
- `web/`: the GUI: `index.html`, `css/app.css` and plain scripts in `js/` loaded in order: `state`
  (shared state), `config` (parsing and validation), `keyboard` (keyboard picture and target
  picker), `actions` (Actions tab), `devices` (server calls), `app` (rendering and start-up).
  `layouts/<os>.json` (`windows`, `macos`) holds what differs per OS: the keyboard picture's rows,
  key and modifier labels, and the Actions tab's note. The GUI loads the one `/api/platform` names.
- `tests/`: C++ tests, one file per area, on a tiny shared harness (`harness.h`: write
  `TEST(name) { CHECK(...); }` and it registers itself). `tests/common/` (`config_`, `matcher_`,
  `keys_`, `engine_`, `static_files_tests.cpp`) runs on every OS; `tests/macos/`
  (`mac_input_tests.cpp`) and `tests/windows/` cover a platform layer and are built on that OS
  only. `tests/web/` has the GUI logic tests (Node).

To port to another OS (for example Linux), add `src/platform/<os>/` implementing `platform.h` and
list it in `CMakeLists.txt`. Nothing in `core/`, `server/`, `web/` or the config format changes.

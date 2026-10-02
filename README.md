# Keymapper

A key remapper for Windows with a browser GUI. Remap keys, define layers (hold a key to change what
other keys do), bind key combos to ready-made actions, and give each physical keyboard its own config.

## Build

Requires Windows, CMake 3.16+ and a C++17 compiler (MSVC).

```
cmake -S . -B build
cmake --build build --config Release
```

The build copies `web/`, the built-in actions (`actions.txt`) and the default config
(`mappings.default.txt`) next to the exe in `build\Release\`.

## Run

```
build\Release\keymapper.exe [config-path] [port]
```

It opens http://127.0.0.1:8765 (the default port) in your default browser. A global keyboard hook is active while it runs,
so remaps apply to every program.

To stop it, click **Stop app** in the GUI's bottom bar (or press Ctrl+C in its console window, or
`taskkill /IM keymapper.exe`). Only one copy runs at a time: starting `keymapper.exe` again stops the
running copy and takes over its port and hook.

The GUI server only listens on localhost and rejects requests from other sites.

## Using the GUI

- **Keyboard** dropdown: choose which keyboard you are editing, or the default config.
  **Detect** selects the keyboard you typed on last.
- **Base** tab: mappings that are always active. A **Layer** tab applies only while its key is held.
- **Keyboard picture**: click a key, then pick what it becomes from the list (actions and keys), or
  use **Pick on keyboard** and click the key. It includes F13-F24 and the media and browser keys.
  Tick Ctrl/Alt/Shift/Win first to bind a combo such as Alt+Q. A combo or steps written by hand in
  the config show as "Custom" in the list and are kept.
- **Actions** tab: every built-in action, grouped by category, with its description and the keys it
  sends. Click **+ Shortcut** on one and press the keys (or tick modifiers and pick a key) to bind it;
  the keys Windows keeps for itself, like Alt+Tab, can't be recorded in a browser, so use the boxes.
  **Remove shortcut** (or a chip's ×) removes only the combo; the action stays. **Change keys** changes
  what an action sends (for example Undo sends `alt+z` instead of `ctrl+z`); **Reset keys** undoes that.
- **Save** applies the change immediately, no restart needed.

## Config files

| File | Used by |
|---|---|
| `mappings.txt` (next to the exe) | the default: every keyboard without its own file |
| `devices\<hardware id>.txt` | one specific keyboard, e.g. `devices\VID_048D&PID_C108.txt` |
| `actions.txt` (next to the exe) | the built-in actions, copied from `config/actions.txt` on every build; do not edit this copy |

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

`from` can be a combo (modifiers `ctrl`, `alt`, `shift`, `win` plus one key) and `to` can be a key, a
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
  writes the `none` line. The shipped `config/actions.txt` declares no defaults at the moment.
- Defining an action with the same name as a built-in one in your config overrides it.

**Built-in actions** (`config/actions.txt`) are 44 common shortcuts in five categories:
- *Windows and desktops:* `switch-window`, `previous-window`, `task-view`, `close-window`,
  `show-desktop`, `lock-screen`, `snap-left`, `next-desktop`, ...
- *Media and browser:* `volume-up`, `volume-down`, `mute`, `play-pause`, `next-track`,
  `previous-track`, `stop-media`, `browser-back`/`-forward`/`-refresh`/`-home`, `launch-mail`.
- *Launch:* `file-explorer`, `run-dialog`, `screenshot-region`, `emoji-picker`, `clipboard-history`.
- *Editing:* `copy`, `cut`, `paste`, `undo`, `redo`, `select-all`, ...
- *Tabs:* `next-tab`, `previous-tab`, `new-tab`, `close-tab`, `reopen-tab`.

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
- Only the Windows platform layer exists today.

## Tests

```
cmake --build build --config Release --target keymapper_tests
build\Release\keymapper_tests.exe
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
  `/api/keys`, `/api/actions`) plus the GUI's scripts and styles from `web/`. It uses
  `std::filesystem` and a small Winsock/BSD sockets shim, so it is portable too.
- `src/platform/platform.h`: what the program needs from the OS (find the install folder, list
  keyboards, intercept keys and inject the engine's output).
  `src/platform/windows/windows_platform.cpp` implements it with a low-level keyboard hook, Raw
  Input (to tell keyboards apart) and `SendInput`.
- `src/main.cpp`: loads configs, creates the engine, starts the server and the platform hook.
- `config/`: `actions.txt` (the built-in actions) and `mappings.txt` (the default config).
- `web/`: the GUI: `index.html`, `css/app.css` and plain scripts in `js/` loaded in order: `state`
  (shared state), `config` (parsing and validation), `keyboard` (keyboard picture and target
  picker), `actions` (Actions tab), `devices` (server calls), `app` (rendering and start-up).
- `tests/`: C++ tests, one file per area (`config_`, `matcher_`, `keys_`, `engine_`,
  `static_files_tests.cpp`) on a tiny shared harness (`harness.h`: write `TEST(name) { CHECK(...); }`
  and it registers itself); `tests/web/` has the GUI logic tests (Node).

To port to another OS (for example macOS), add `src/platform/<os>/` implementing `platform.h` and
list it in `CMakeLists.txt`. Nothing in `core/`, `server/`, `web/` or the config format changes.

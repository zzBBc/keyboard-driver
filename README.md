# Keymapper

A small Windows key remapper with a browser GUI. Remap keys, define layers (hold a key to
change what other keys do), and give each physical keyboard its own config.

## Build

Requires Windows, CMake 3.16+ and a C++17 compiler (MSVC).

```
cmake -S . -B build
cmake --build build --config Release
```

The build copies `web/` and the default config next to the exe in `build\Release\`.

## Run

```
build\Release\keymapper.exe [config-path] [port]
```

Then open http://127.0.0.1:8765 (the default port). Press Ctrl+C to quit.

The GUI only listens on localhost. A global keyboard hook is active while it runs, so
remaps apply to every program.

## Using the GUI

- **Keyboard** dropdown: choose which keyboard you are editing, or the default config.
  **Detect** selects the keyboard you typed on last.
- **Tabs**: `Base` mappings are always active. A `Layer` tab applies only while its key is held.
- **Keyboard picture**: click a key, then pick what it becomes from the list (actions and keys) or use **Pick on keyboard**. A combo or steps written by hand in the config show as "Custom" and are kept.
- **Save** applies the change immediately, no restart needed.

## Config files

| File | Used by |
|---|---|
| `mappings.txt` (next to the exe) | the default: every keyboard without its own file |
| `devices\<hardware id>.txt` | one specific keyboard, e.g. `devices\VID_048D&PID_C108.txt` |

On first run `mappings.txt` is created from `mappings.default.txt` and never overwritten.
Both are edited by the GUI, or by hand (restart to pick up hand edits).

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

`from` can be a combo (modifiers `ctrl`, `alt`, `shift`, `win` plus one key) and `to` can be a
key, a combo, comma-separated steps, or `@action`. An action is a named list of steps, defined
once and bound anywhere:

```
alt+q = @switch-window       # Alt+Q runs the action
alt+w = ctrl+shift+tab       # or a combo directly

[action switch-window]
alt+tab

[action copy-paste]
ctrl+c, ctrl+v               # steps run in order
```

- A combo fires only when exactly those modifiers are held, and the trigger key is swallowed.
- If a step wants a modifier you are already holding (Alt for `alt+tab`), it is left held, so
  the Windows switcher stays open while you keep Alt down. Other held modifiers are released
  for the step and put back afterwards.
- Optional `description: text` as the first line of an action shows in the GUI. A
  `category: Name` line (library actions) groups the action in the GUI's Actions tab and action
  list, in the order the file lists them.
- A library action can declare default combos with `shortcut: alt+q` lines. They are bound in
  Base unless your config binds that combo itself or clears it with `alt+q = none`. In the
  GUI a default shows as a chip marked "(default)"; its × writes the `none` line. The shipped
  `config/actions.txt` currently declares no defaults.
- A `from = to` line after an action block ends that block. Actions work in base, layers and
  per-keyboard files. The GUI's **Actions** tab lists every action; click **+ Shortcut** on one
  and press the keys (or tick Ctrl/Alt/Shift/Win and pick a key) to bind it. **Remove shortcut**
  (or a chip's ×) removes only the key combo; the action stays in the list. The keyboard
  picture also has Ctrl/Alt/Shift/Win checkboxes for binding combos.
- **Change keys** on an action changes what it sends (for example Undo sends `alt+z` instead of
  `ctrl+z`). It is saved as your own `[action undo]` block, which overrides the built-in one;
  **Reset keys** deletes that block. Its shortcuts, including defaults, keep working.

**Built-in actions** (`config/actions.txt`, copied next to the exe on every build) are about 30
common shortcuts such as `switch-window`, `close-window`, `show-desktop`, `copy`, `next-tab`.
Use them directly (`alt+q = @switch-window`) without defining them. An action of the same name
in your own config wins. The GUI lists them, with descriptions, on the Actions tab and in the
"becomes" list. Actions are chosen from these lists, not created in the GUI; to define your
own, write an `[action ...]` block in the config file.

Media actions (`volume-up`, `volume-down`, `mute`, `play-pause`, `next-track`, `previous-track`,
`stop-media`, `browser-back`/`-forward`/`-refresh`/`-home`, `launch-mail`) send the same keys as a
keyboard's media keys and have no default shortcut. Use them to put a media function on a combo of
your choice, for example `alt+up = @volume-up`. They can't send a key your keyboard reports
only as a vendor-specific report, and screen brightness has no key that Windows accepts, so
there is no brightness action.

Key names are lowercase: letters, digits, `f1`-`f24`, `esc`, `tab`, `capslock`, `space`,
`enter`, `backspace`, `lshift`/`rshift`, `lctrl`/`rctrl`, `lalt`/`ralt`, `lwin`/`rwin`,
arrows, `home`/`end`/`pageup`/`pagedown`, `insert`/`delete`, punctuation, and so on. The GUI
autocompletes the full list.

## Per-keyboard configs

Keyboards are told apart by hardware ID (USB `VID_xxxx&PID_xxxx`, or the bus ID for
built-in keyboards such as `FUJ7401`). The low-level hook can't say which keyboard sent a
key, so Raw Input events are matched to hook events by scan code and timing.

Limits:

- Two identical keyboards (same VID/PID) share one config.
- If two keyboards press the same key at nearly the same instant, one may be misattributed.
- Layer keys are tracked per keyboard.

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

To port to another OS (for example macOS), add `src/platform/<os>/` implementing `platform.h` and
list it in `CMakeLists.txt`. Nothing in `core/`, `server/`, `web/` or the config format changes.
- `config/actions.txt`: built-in actions library
- `tests/`: C++ tests, one file per area (`config_`, `matcher_`, `keys_`, `engine_`, `static_files_tests.cpp`)
  on a tiny shared harness (`harness.h`: write `TEST(name) { CHECK(...); }`, it registers itself);
  `tests/web/` has the GUI logic tests (Node)
- `web/index.html`, `web/css/app.css`, `web/js/*.js`: the GUI. Plain scripts loaded in order:
  `state` (shared state), `config` (parsing and validation), `keyboard` (keyboard picture and
  target picker), `actions` (Actions tab), `devices` (server calls), `app` (rendering and start-up)

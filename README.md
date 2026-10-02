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
- Optional `description: text` as the first line of an action shows in the GUI.
- A `from = to` line after an action block ends that block. Actions work in base, layers and
  per-keyboard files. The GUI's **Actions** tab lists every action; click **+ Shortcut** on one
  and press the keys (or tick Ctrl/Alt/Shift/Win and pick a key) to bind it. The keyboard
  picture also has Ctrl/Alt/Shift/Win checkboxes for binding combos.

**Built-in actions** (`config/actions.txt`, copied next to the exe on every build) are about 30
common shortcuts such as `switch-window`, `close-window`, `show-desktop`, `copy`, `next-tab`.
Use them directly (`alt+q = @switch-window`) without defining them. An action of the same name
in your own config wins. The GUI lists them, with descriptions, on the Actions tab and in the
"becomes" list. Actions are chosen from these lists, not created in the GUI; to define your
own, write an `[action ...]` block in the config file.

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

## Layout

- `src/hook.cpp`: keyboard hook, Raw Input attribution, remapping
- `src/server.cpp`: localhost HTTP server and API (`/api/config`, `/api/devices`, `/api/keys`)
- `src/config_loader.cpp`, `src/key_names.cpp`: config parsing and key names
- `src/matcher.cpp`: chord matching and key-event expansion (unit tested)
- `config/actions.txt`: built-in actions library
- `web/index.html`: the GUI

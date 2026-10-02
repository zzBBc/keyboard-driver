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
- **Keyboard picture**: click a key, then type its new target or use **Pick on keyboard**.
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

## Layout

- `src/hook.cpp`: keyboard hook, Raw Input attribution, remapping
- `src/server.cpp`: localhost HTTP server and API (`/api/config`, `/api/devices`, `/api/keys`)
- `src/config_loader.cpp`, `src/key_names.cpp`: config parsing and key names
- `web/index.html`: the GUI

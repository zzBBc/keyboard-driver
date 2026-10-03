# Run the release zip

The release builds are in the repository's `release/<os>/`: `keymapper-windows.zip` and
`keymapper-macos.zip`, and `keymapper-android.apk` for Android tablets (see [Android](#android)). Each zip holds the program, `web/`, the built-in actions, the default config and
this file.

## Windows

1. Unzip it to a folder of your own, for example `%LOCALAPPDATA%\keymapper`.
2. Start `keymapper.exe`. The program is not signed, so Windows may show "Windows protected your PC":
   click **More info**, then **Run anyway**.
3. It opens http://127.0.0.1:8765 in your browser.
4. Stop it with **Stop app** in the GUI's bottom bar, Ctrl+C in its console window, or
   `taskkill /IM keymapper.exe`. Starting it again stops the running copy.

## macOS

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
5. Stop it with **Stop app** in the GUI's bottom bar, Ctrl+C, or `pkill keymapper`. Starting it
   again stops the running copy.

A new release replaces the program, and macOS may ask for the permissions again. If remapping stops
working after an update, remove the old entry with **-** in both lists and grant it again.

## Android

For a tablet with a hardware keyboard (Android 9 or newer). Details and limits: [android/README.md](android/README.md).

1. Copy `release/android/keymapper-android.apk` to the tablet and open it. The APK is signed with a debug
   key, so allow installing from this source when Android asks.
2. Open **Keymapper**, tap **Open accessibility settings**, and turn on the **Keymapper** service.
3. Open **Keymapper** again: the GUI appears. **Stop app** turns the service off.

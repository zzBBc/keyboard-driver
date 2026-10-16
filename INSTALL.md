# Run the release zip

The release builds are in the repository's `release/<os>/`: `keymapper-windows-setup.exe`,
`keymapper-windows.zip` and `keymapper-macos.zip`, and `keymapper-android.apk` for Android tablets (see [Android](#android)). Each zip holds the program, `web/`, the built-in actions, the default config and
this file.

## Windows

**Installer (recommended)**: run `keymapper-windows-setup.exe`. It installs to `%LOCALAPPDATA%\keymapper` (no admin
needed) and adds a Start menu entry. Run a newer installer to update: your config (`mappings.txt`, `devices\`) is
kept, and uninstalling leaves it too. Unsigned, so see the "Windows protected your PC" note below.

**Zip**: to update, unzip the new release over the old folder (do not delete the folder first). The zip has no `mappings.txt` or `devices\`, so your config is kept.

1. Unzip it to a folder of your own, for example `%LOCALAPPDATA%\keymapper`.
2. Start `keymapper.exe`. The program is not signed, so Windows may show "Windows protected your PC":
   click **More info**, then **Run anyway**.
3. It opens http://127.0.0.1:8765 in your browser.
4. Stop it with **Stop app** in the GUI's bottom bar, or `taskkill /IM keymapper.exe`
   (it has no console window). Starting it again stops the running copy.

## macOS

1. Unzip it and move `Keymapper.app` out of Downloads, for example to `~/Applications`. macOS
   ties the permissions below to the program's path, so keep it in one place.
2. The zip is not signed, so macOS says it "could not verify keymapper is free of malware". Clear the
   download quarantine from the app:

   ```
   xattr -dr com.apple.quarantine ~/Applications/Keymapper.app
   ```

   If `xattr` says "Operation not permitted", your terminal app may not have access to that folder:
   grant it under **System Settings > Privacy & Security > Files and Folders** (or **Full Disk
   Access**), or run the command from another terminal. Instead of `xattr`, you can also start
   keymapper once, then click **Open Anyway** in **System Settings > Privacy & Security**.
3. Start it by opening `Keymapper.app`: it has no Dock icon and no Terminal window, and the GUI opens in your
   browser. On first start macOS asks for two permissions. Turn both on
   in **System Settings > Privacy & Security**:
   - **Accessibility**: to intercept and send keys. Remapping does nothing without it.
   - **Input Monitoring**: to tell keyboards apart. Without it every keyboard uses the default config.

   If the app is not in a list, click **+** and add it.
4. Restart keymapper after granting the permissions.
5. Stop it with **Stop app** in the GUI's bottom bar or `pkill keymapper`. Starting it
   again stops the running copy.

Your config is kept in `~/Library/Application Support/keymapper`, outside the app, so replace `Keymapper.app`
with a newer one to update. macOS may ask for the permissions again. If remapping stops
working after an update, remove the old entry with **-** in both lists and grant it again.

## Android

For a tablet with a hardware keyboard (Android 9 or newer). Details and limits: [android/README.md](android/README.md).

1. Copy `release/android/keymapper-android.apk` to the tablet and open it. The APK is signed with a debug
   key, so allow installing from this source when Android asks.
2. Open **Keymapper**, tap **Open accessibility settings**, and turn on the **Keymapper** service.
3. Open **Keymapper** again: the GUI appears. **Stop app** turns the service off.

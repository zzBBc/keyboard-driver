package io.keymapper

import android.accessibilityservice.AccessibilityService
import android.content.Context
import android.hardware.input.InputManager
import android.os.Process
import android.view.InputDevice
import android.view.KeyEvent
import android.view.accessibility.AccessibilityEvent
import java.io.File

/**
 * The keyboard hook: Android gives an accessibility service every hardware-keyboard event before
 * the app in front sees it, and lets it swallow them. Each event goes to the native engine, which
 * says whether to swallow it and which global actions (Back, Home, ...) to run instead.
 */
class KeymapperService : AccessibilityService(), InputManager.InputDeviceListener {

    override fun onServiceConnected() {
        instance = this
        copyAssets()
        refreshKeyboards()
        (getSystemService(Context.INPUT_SERVICE) as InputManager).registerInputDeviceListener(this, null)
        // Turning the service off and on again in Settings reconnects it in the same process, where the
        // program (engine and GUI server) is still running: start it only once.
        if (!nativeStarted) {
            nativeStarted = true
            Thread {
                NativeBridge.run(filesDir.absolutePath)  // returns after the GUI's Stop button
                // The GUI server cannot be restarted in this process, so end it; the user turns the service on again.
                disableSelf()
                Process.killProcess(Process.myPid())
            }.start()
        }
    }

    override fun onKeyEvent(event: KeyEvent): Boolean {
        val up = event.action == KeyEvent.ACTION_UP
        val result = NativeBridge.onKey(deviceId(event.device), event.keyCode, up)
        for (i in 1 until result.size) performGlobalAction(result[i])
        return result[0] == 1
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {}

    override fun onInterrupt() {}

    override fun onDestroy() {
        if (instance === this) instance = null
        (getSystemService(Context.INPUT_SERVICE) as InputManager).unregisterInputDeviceListener(this)
        super.onDestroy()
    }

    override fun onInputDeviceAdded(deviceId: Int) = refreshKeyboards()
    override fun onInputDeviceRemoved(deviceId: Int) = refreshKeyboards()
    override fun onInputDeviceChanged(deviceId: Int) = refreshKeyboards()

    private fun refreshKeyboards() {
        val keyboards = InputDevice.getDeviceIds().toList().mapNotNull { InputDevice.getDevice(it) }.filter { isKeyboard(it) }
        // One entry per hardware id, like the other platforms.
        val unique = keyboards.associateBy { deviceId(it) }.filterKeys { it.isNotEmpty() }
        NativeBridge.setKeyboards(unique.keys.toTypedArray(), unique.map { it.value.name }.toTypedArray())
    }

    /** The web GUI and config files are the repository's own, packaged as assets; refresh the copies. */
    private fun copyAssets() {
        copyAsset("web")
        copyAsset("actions.android.txt")
        copyAsset("mappings.default.txt")
    }

    private fun copyAsset(name: String, target: File = File(filesDir, name)) {
        val children = assets.list(name)
        if (children != null && children.isNotEmpty()) {
            target.mkdirs()
            for (child in children) copyAsset("$name/$child", File(target, child))
        } else {
            assets.open(name).use { input -> target.outputStream().use { input.copyTo(it) } }
        }
    }

    companion object {
        /** Set while the service is on, so the activity can tell whether the hook is running. */
        @Volatile var instance: KeymapperService? = null

        /** Whether the native program was started in this process. */
        @Volatile private var nativeStarted = false

        private fun isKeyboard(d: InputDevice) =
            !d.isVirtual && d.sources and InputDevice.SOURCE_KEYBOARD == InputDevice.SOURCE_KEYBOARD &&
                d.keyboardType == InputDevice.KEYBOARD_TYPE_ALPHABETIC

        /** Same format as the other platforms' device file names; "" (the default config) when unknown. */
        fun deviceId(d: InputDevice?): String =
            if (d == null || d.vendorId == 0 && d.productId == 0) ""
            else "VID_%04X&PID_%04X".format(d.vendorId, d.productId)
    }
}

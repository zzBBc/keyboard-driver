package io.keymapper

/** The native half (src/platform/android/android_platform.cpp). */
object NativeBridge {
    init {
        System.loadLibrary("c++_shared")
        System.loadLibrary("keymapper")
    }

    /** Runs the program with [dir] as its folder. Blocks until [quit], so call it on its own thread. */
    @JvmStatic external fun run(dir: String): Int

    @JvmStatic external fun quit()

    @JvmStatic external fun setKeyboards(ids: Array<String>, names: Array<String>)

    /** {swallow (1 or 0), global action, global action, ...} for one physical key event. */
    @JvmStatic external fun onKey(device: String, keyCode: Int, up: Boolean): IntArray
}

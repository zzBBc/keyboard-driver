// Android layer. The app is a Kotlin accessibility service (android/) that owns the keyboard hook:
// it hands each hardware-keyboard event to onKey() below and swallows it or runs global actions as
// told. This file is the native half: it runs the portable program (main.cpp, renamed
// keymapper_main by CMake) on a thread of the service, and answers the platform.h calls.
#include <jni.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include "android_input.h"
#include "engine.h"
#include "platform.h"

int keymapper_main(int argc, char** argv);

namespace {

std::mutex g_mutex;  // guards everything below
std::condition_variable g_quitCv;
bool g_quit = false;
std::string g_dir;
std::string g_lastKeyboard;
std::vector<KeyboardInfo> g_keyboards;
Engine* g_engine = nullptr;

std::string toString(JNIEnv* env, jstring s) {
    if (!s) return "";
    const char* chars = env->GetStringUTFChars(s, nullptr);
    std::string out = chars ? chars : "";
    if (chars) env->ReleaseStringUTFChars(s, chars);
    return out;
}

}  // namespace

namespace platform {

const char* name() { return "android"; }

std::string exeDir() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_dir;
}

std::vector<KeyboardInfo> listKeyboards() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_keyboards;
}

std::string lastKeyboard() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_lastKeyboard;
}

// The service is the single instance: Android starts it once.
bool takeOverFromRunningInstance() { return true; }

// The app shows the GUI itself, in a WebView.
bool openUrl(const std::string&) { return true; }

void adoptConfig(const std::string&, Config&) {}

void requestQuit() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_quit = true;
    }
    g_quitCv.notify_all();
}

int runHook(Engine& engine) {
    std::unique_lock<std::mutex> lock(g_mutex);
    g_engine = &engine;
    g_quitCv.wait(lock, [] { return g_quit; });
    g_engine = nullptr;
    return 0;
}

}  // namespace platform

extern "C" {

// Runs the program with `dir` as its folder (web/, actions.android.txt, mappings.default.txt and the
// configs live there). Blocks until requestQuit(), so call it from a thread of its own.
JNIEXPORT jint JNICALL Java_io_keymapper_NativeBridge_run(JNIEnv* env, jclass, jstring dir) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_dir = toString(env, dir);
        g_quit = false;
    }
    char arg0[] = "keymapper";
    char* argv[] = {arg0, nullptr};
    return keymapper_main(1, argv);
}

JNIEXPORT void JNICALL Java_io_keymapper_NativeBridge_quit(JNIEnv*, jclass) { platform::requestQuit(); }

// ids[i] and names[i] describe one attached hardware keyboard.
JNIEXPORT void JNICALL Java_io_keymapper_NativeBridge_setKeyboards(JNIEnv* env, jclass, jobjectArray ids,
                                                                    jobjectArray names) {
    std::vector<KeyboardInfo> list;
    const jsize n = ids ? env->GetArrayLength(ids) : 0;
    for (jsize i = 0; i < n; i++) {
        auto id = static_cast<jstring>(env->GetObjectArrayElement(ids, i));
        auto name = static_cast<jstring>(env->GetObjectArrayElement(names, i));
        list.push_back({toString(env, id), toString(env, name)});
        env->DeleteLocalRef(id);
        env->DeleteLocalRef(name);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_keyboards = std::move(list);
}

// One physical key event from the service. Returns {swallow, action, action, ...}: swallow is 1 to
// consume the event; each action is an AccessibilityService global action to run, in order. Called
// from the service's main thread only, which is the one thread the engine allows.
JNIEXPORT jintArray JNICALL Java_io_keymapper_NativeBridge_onKey(JNIEnv* env, jclass, jstring device,
                                                                  jint keyCode, jboolean up) {
    android::Decision decision;
    const unsigned short id = android::fromAndroid(keyCode);
    const std::string dev = toString(env, device);
    Engine* engine;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        engine = g_engine;
        if (id && !up) g_lastKeyboard = dev;
    }
    if (engine && id) {
        std::vector<KeyEvent> send;
        const bool swallow = engine->handle(dev, id, up, send);
        decision = android::decide(swallow, send);
    }
    std::vector<jint> out{decision.swallow ? 1 : 0};
    out.insert(out.end(), decision.actions.begin(), decision.actions.end());
    jintArray result = env->NewIntArray(static_cast<jsize>(out.size()));
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(out.size()), out.data());
    return result;
}

}  // extern "C"

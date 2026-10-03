// The Android layer's decisions: key code table, which keys stand for global actions, and what the
// accessibility service does with the engine's answer. No Android API is used, so this runs on every OS.
#include "harness.h"
#include "helpers.h"

#include "android_input.h"
#include "keycodes.h"

TEST(androidKeyCodesMapToPortableIds) {
    CHECK(android::fromAndroid(29) == 'A' && android::fromAndroid(54) == 'Z');  // KEYCODE_A, KEYCODE_Z
    CHECK(android::fromAndroid(7) == '0' && android::fromAndroid(16) == '9');   // KEYCODE_0, KEYCODE_9
    CHECK(android::fromAndroid(131) == key::F1 && android::fromAndroid(142) == key::F1 + 11);
    CHECK(android::fromAndroid(61) == key::Tab && android::fromAndroid(115) == key::CapsLock);
    CHECK(android::fromAndroid(113) == key::LControl && android::fromAndroid(117) == key::LWin);
    CHECK(android::fromAndroid(android::KeycodeBack) == key::BrowserBack);
    CHECK(android::fromAndroid(android::KeycodeHome) == key::BrowserHome);
    CHECK(android::fromAndroid(0) == 0 && android::fromAndroid(9999) == 0);  // unknown: never remapped
}

TEST(androidGlobalActionKeys) {
    CHECK(android::globalActionFor(key::BrowserBack) == android::ActionBack);
    CHECK(android::globalActionFor(key::BrowserHome) == android::ActionHome);
    CHECK(android::globalActionFor(key::Apps) == android::ActionRecents);
    CHECK(android::globalActionFor(key::PrintScreen) == android::ActionTakeScreenshot);
    CHECK(android::globalActionFor('A') == 0 && android::globalActionFor(key::Left) == 0);
}

TEST(androidDecisionRunsActionsOnPressOnly) {
    auto d = android::decide(true, {{key::BrowserBack, false}, {key::BrowserBack, true}});
    CHECK(d.swallow && d.actions.size() == 1 && d.actions[0] == android::ActionBack);
}

TEST(androidDecisionSwallowsWithoutSending) {
    auto d = android::decide(true, {});  // a layer key, or a key mapped to nothing
    CHECK(d.swallow && d.actions.empty());
}

TEST(androidDecisionKeepsTheKeyWhenItCannotSendWhatIsAsked) {
    auto d = android::decide(true, {{key::Left, false}, {key::Left, true}});  // no key injection without root
    CHECK(!d.swallow && d.actions.empty());
    auto mixed = android::decide(true, {{key::BrowserHome, false}, {'A', false}});
    CHECK(!mixed.swallow && mixed.actions.empty());  // all or nothing
}

TEST(androidDecisionLeavesAnUnhandledKeyAlone) {
    auto d = android::decide(false, {});
    CHECK(!d.swallow && d.actions.empty());
}

TEST(androidEngineBindingRunsAGlobalAction) {
    Engine e;
    auto lib = parse(sourceFile("config/actions.android.txt"));
    CHECK(lib.ok);
    auto cfg = std::make_shared<Config>();
    std::vector<std::string> errors;
    CHECK(parseConfig("alt+q = @back\n", *cfg, errors, &lib.cfg));
    e.setConfig("", cfg);
    std::vector<KeyEvent> send;
    e.handle("K1", key::LAlt, false, send);
    send.clear();
    const bool swallow = e.handle("K1", 'Q', false, send);
    auto d = android::decide(swallow, send);
    CHECK(d.swallow && d.actions.size() == 1 && d.actions[0] == android::ActionBack);
}

TEST(androidDecisionIgnoresModifiersAroundAnAction) {
    auto d = android::decide(true, {{key::LAlt, true}, {key::BrowserBack, false}, {key::BrowserBack, true}, {key::LAlt, false}});
    CHECK(d.swallow && d.actions.size() == 1);
    auto onlyMods = android::decide(true, {{key::LShift, false}});  // nothing we can carry out
    CHECK(!onlyMods.swallow);
}

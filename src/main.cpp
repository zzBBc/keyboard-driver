#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <thread>

#include "config_loader.h"
#include "engine.h"
#include "platform.h"
#include "server.h"

int main(int argc, char** argv) {
    // A second launch replaces the running copy (stops it, then takes over its port and hook).
    if (!platform::takeOverFromRunningInstance()) return 1;

    const std::string dir = platform::exeDir();
    Engine engine;

    // Built-in actions shipped next to the exe; configs may use them without defining them. Each OS
    // has its own list (they send that OS's shortcuts): actions.windows.txt, actions.macos.txt.
    auto library = std::make_shared<Config>();
    {
        std::string libText;
        std::vector<std::string> libErrors;
        const std::string libPath = (std::filesystem::path(dir) / (std::string("actions.") + platform::name() + ".txt")).string();
        if (readFile(libPath, libText) && !parseConfig(libText, *library, libErrors))
            for (const auto& e : libErrors) std::cerr << libPath << ": " << e << "\n";
    }

    ServerOptions opts;
    opts.engine = &engine;
    opts.onQuit = [] {  // the GUI's Stop button: let the reply go out, then stop the hook
        std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            platform::requestQuit();
        }).detach();
    };
    opts.library = library;
    opts.webDir = (std::filesystem::path(dir) / "web").string();
    opts.devicesDir = (std::filesystem::path(dir) / "devices").string();
    opts.configPath = argc > 1 ? argv[1] : (std::filesystem::path(dir) / "mappings.txt").string();
    if (argc > 2) opts.port = std::atoi(argv[2]);

    // First run: seed the config from the bundled default.
    std::string text;
    if (!readFile(opts.configPath, text)) {
        if (readFile((std::filesystem::path(dir) / "mappings.default.txt").string(), text)) writeFile(opts.configPath, text);
    }

    auto cfg = std::make_shared<Config>();
    std::vector<std::string> errors;
    if (!parseConfig(text, *cfg, errors, library.get()))
        for (const auto& e : errors) std::cerr << opts.configPath << ": " << e << "\n";
    platform::adoptConfig("", *cfg);
    engine.setConfig("", cfg);

    // Per-keyboard configs: devices\<hardware id>.txt
    std::error_code scanError;
    for (const auto& entry : std::filesystem::directory_iterator(opts.devicesDir, scanError)) {
        if (entry.path().extension() != ".txt") continue;
        const std::string id = entry.path().stem().string();
        const std::string path = entry.path().string();
        std::string devText;
        auto devCfg = std::make_shared<Config>();
        std::vector<std::string> devErrors;
        if (!validDeviceId(id) || !readFile(path, devText)) continue;
        if (!parseConfig(devText, *devCfg, devErrors, library.get()))
            for (const auto& e : devErrors) std::cerr << path << ": " << e << "\n";
        platform::adoptConfig(id, *devCfg);
        engine.setConfig(id, devCfg);
        std::cout << "Keyboard config: " << id << "\n";
    }

    if (!startServer(opts)) {
        std::cerr << "Cannot listen on 127.0.0.1:" << opts.port << " (already running?)\n";
        return 1;
    }
    const std::string url = "http://127.0.0.1:" + std::to_string(opts.port);
    std::cout << "Config: " << opts.configPath << "\n"
              << "Open " << url << " to edit mappings. Ctrl+C to quit.\n";
    platform::openUrl(url);
    return platform::runHook(engine);
}

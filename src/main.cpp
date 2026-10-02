#include <windows.h>

#include <iostream>
#include <memory>

#include "config_loader.h"
#include "engine.h"
#include "platform.h"
#include "server.h"

int main(int argc, char** argv) {
    const std::string dir = platform::exeDir();
    Engine engine;

    // Built-in actions shipped next to the exe; configs may use them without defining them.
    auto library = std::make_shared<Config>();
    {
        std::string libText;
        std::vector<std::string> libErrors;
        const std::string libPath = dir + "\\actions.txt";
        if (readFile(libPath, libText) && !parseConfig(libText, *library, libErrors))
            for (const auto& e : libErrors) std::cerr << libPath << ": " << e << "\n";
    }

    ServerOptions opts;
    opts.engine = &engine;
    opts.library = library;
    opts.webDir = dir + "\\web";
    opts.devicesDir = dir + "\\devices";
    opts.configPath = argc > 1 ? argv[1] : dir + "\\mappings.txt";
    if (argc > 2) opts.port = std::atoi(argv[2]);

    // First run: seed the config from the bundled default.
    std::string text;
    if (!readFile(opts.configPath, text)) {
        if (readFile(dir + "\\mappings.default.txt", text)) writeFile(opts.configPath, text);
    }

    auto cfg = std::make_shared<Config>();
    std::vector<std::string> errors;
    if (!parseConfig(text, *cfg, errors, library.get()))
        for (const auto& e : errors) std::cerr << opts.configPath << ": " << e << "\n";
    engine.setConfig("", cfg);

    // Per-keyboard configs: devices\<hardware id>.txt
    WIN32_FIND_DATAA fd;
    if (HANDLE h = FindFirstFileA((opts.devicesDir + "\\*.txt").c_str(), &fd); h != INVALID_HANDLE_VALUE) {
        do {
            const std::string id = std::string(fd.cFileName).substr(0, std::string(fd.cFileName).size() - 4);
            const std::string path = opts.devicesDir + "\\" + fd.cFileName;
            std::string devText;
            auto devCfg = std::make_shared<Config>();
            std::vector<std::string> devErrors;
            if (!validDeviceId(id) || !readFile(path, devText)) continue;
            if (!parseConfig(devText, *devCfg, devErrors, library.get()))
                for (const auto& e : devErrors) std::cerr << path << ": " << e << "\n";
            engine.setConfig(id, devCfg);
            std::cout << "Keyboard config: " << id << "\n";
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    if (!startServer(opts)) {
        std::cerr << "Cannot listen on 127.0.0.1:" << opts.port << " (already running?)\n";
        return 1;
    }
    std::cout << "Config: " << opts.configPath << "\n"
              << "Open http://127.0.0.1:" << opts.port << " to edit mappings. Ctrl+C to quit.\n";
    return platform::runHook(engine);
}

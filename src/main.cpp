#include <windows.h>

#include <iostream>
#include <memory>

#include "config_loader.h"
#include "hook.h"
#include "server.h"

namespace {

std::string exeDir() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf, n);
    return path.substr(0, path.find_last_of("\\/"));
}

}  // namespace

int main(int argc, char** argv) {
    const std::string dir = exeDir();

    ServerOptions opts;
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
    if (!parseConfig(text, *cfg, errors))
        for (const auto& e : errors) std::cerr << opts.configPath << ": " << e << "\n";
    setConfig("", cfg);

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
            if (!parseConfig(devText, *devCfg, devErrors))
                for (const auto& e : devErrors) std::cerr << path << ": " << e << "\n";
            setConfig(id, devCfg);
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
    return runHook();
}

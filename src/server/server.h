#pragma once
#include <functional>
#include <memory>
#include <string>

#include "config_loader.h"
#include "engine.h"

struct ServerOptions {
    int port = 8765;
    Engine* engine = nullptr;  // receives saved configs
    std::function<void()> onQuit;  // called after POST /api/quit has been answered
    std::string webDir;      // folder containing index.html
    std::string configPath;  // default config, read/written by the GUI
    std::shared_ptr<const Config> library;  // built-in actions (config/actions.<os>.txt)
    std::string devicesDir;  // per-keyboard configs: <devicesDir>\<hardware id>.txt
};

// Start the localhost-only HTTP server on a background thread.
// Saving through the API validates, writes the file and updates the engine.
// Config endpoints take an optional ?device=<hardware id>; without it they address the default.
bool startServer(const ServerOptions& opts);

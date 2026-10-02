#pragma once
#include <string>

struct ServerOptions {
    int port = 8765;
    std::string webDir;      // folder containing index.html
    std::string configPath;  // default config, read/written by the GUI
    std::string devicesDir;  // per-keyboard configs: <devicesDir>\<hardware id>.txt
};

// Start the localhost-only HTTP server on a background thread.
// Saving through the API validates, writes the file and calls setConfig().
// Config endpoints take an optional ?device=<hardware id>; without it they address the default.
bool startServer(const ServerOptions& opts);

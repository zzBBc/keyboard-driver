#include "server.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

#include "config_loader.h"
#include "platform.h"
#include "key_names.h"
#include "static_files.h"

// Sockets: Winsock on Windows, BSD sockets elsewhere. Only these few calls differ.
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace {
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
void closeSocket(socket_t s) { closesocket(s); }
void setReceiveTimeout(socket_t s, int ms) {
    DWORD t = static_cast<DWORD>(ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&t), sizeof t);
}
void allowQuickRestart(socket_t) {}  // SO_REUSEADDR means something else (port hijacking) on Windows
bool startNetworking() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}
}  // namespace
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
void closeSocket(socket_t s) { close(s); }
void setReceiveTimeout(socket_t s, int ms) {
    timeval t{ms / 1000, (ms % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);
}
// Lets a restarted copy bind the port while connections of the previous one are still closing (TIME_WAIT).
void allowQuickRestart(socket_t s) {
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
}
bool startNetworking() {
    signal(SIGPIPE, SIG_IGN);  // a browser that hangs up mid-reply must not kill the program
    return true;
}
}  // namespace
#endif

namespace {

constexpr size_t kMaxBody = 1 << 20;

struct Request {
    std::string method, path, body;
    std::map<std::string, std::string> headers;  // lower-case names
};

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

std::string urlDecode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(in[i + 2]))) {
            out += static_cast<char>(std::stoi(in.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else {
            out += in[i];
        }
    }
    return out;
}

bool readRequest(socket_t s, Request& req) {
    std::string buf;
    char tmp[4096];
    size_t headEnd;
    while ((headEnd = buf.find("\r\n\r\n")) == std::string::npos) {
        if (buf.size() > 16 * 1024) return false;
        int n = recv(s, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        buf.append(tmp, n);
    }

    std::string head = buf.substr(0, headEnd);
    std::string body = buf.substr(headEnd + 4);

    size_t lineEnd = head.find("\r\n");
    std::string requestLine = head.substr(0, lineEnd);
    size_t sp1 = requestLine.find(' '), sp2 = requestLine.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
    req.method = requestLine.substr(0, sp1);
    req.path = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);

    while (lineEnd != std::string::npos) {
        size_t start = lineEnd + 2;
        lineEnd = head.find("\r\n", start);
        std::string line = head.substr(start, lineEnd == std::string::npos ? std::string::npos : lineEnd - start);
        if (auto colon = line.find(':'); colon != std::string::npos)
            req.headers[lower(line.substr(0, colon))] = trim(line.substr(colon + 1));
    }

    size_t want = 0;
    if (auto it = req.headers.find("content-length"); it != req.headers.end()) {
        want = static_cast<size_t>(std::strtoull(it->second.c_str(), nullptr, 10));
        if (want > kMaxBody) return false;
    }
    while (body.size() < want) {
        int n = recv(s, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        body.append(tmp, n);
    }
    body.resize(want);
    req.body = std::move(body);
    return true;
}

void respond(socket_t s, int status, const char* reason, const char* type, const std::string& body) {
    std::string out = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n"
                      "Content-Type: " + type + "\r\n"
                      "Content-Length: " + std::to_string(body.size()) + "\r\n"
                      "Cache-Control: no-store\r\n"
                      "Connection: close\r\n\r\n" + body;
    send(s, out.data(), static_cast<int>(out.size()), 0);
}

struct Target {
    std::string path, query;
};

Target splitTarget(const std::string& raw) {
    auto q = raw.find('?');
    if (q == std::string::npos) return {raw, ""};
    return {raw.substr(0, q), raw.substr(q + 1)};
}

// Hardware ids contain only [A-Z0-9_&], so no URL-decoding is needed (anything else fails validation).
std::string queryParam(const std::string& query, const std::string& name) {
    size_t pos = 0;
    while (pos <= query.size()) {
        size_t end = query.find('&', pos);
        // "&" inside an id would split here, so ids are sent with %26 and decoded below.
        std::string pair = query.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (pair.rfind(name + "=", 0) == 0) return urlDecode(pair.substr(name.size() + 1));
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    return "";
}

// Steps back as config text, e.g. "ctrl+c, ctrl+v".
std::string stepsToText(const Steps& steps) {
    std::string out;
    for (const auto& step : steps) {
        if (!out.empty()) out += ", ";
        if (step.mods & ModFn) out += "fn+";
        if (step.mods & ModCtrl) out += "ctrl+";
        if (step.mods & ModAlt) out += "alt+";
        if (step.mods & ModShift) out += "shift+";
        if (step.mods & ModWin) out += "win+";
        out += nameFromVk(step.key);
    }
    return out;
}

std::string deviceConfigPath(const ServerOptions& opts, const std::string& id) {
    return (std::filesystem::path(opts.devicesDir) / (id + ".txt")).string();
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

void handle(socket_t s, const ServerOptions& opts) {
    Request req;
    if (!readRequest(s, req)) return;

    // Anti-CSRF / DNS-rebinding: only accept requests addressed to us, from our own page.
    const std::string port = std::to_string(opts.port);
    const std::string host = req.headers["host"];
    if (host != "127.0.0.1:" + port && host != "localhost:" + port)
        return respond(s, 403, "Forbidden", "text/plain", "bad host");
    if (auto it = req.headers.find("origin"); it != req.headers.end())
        if (it->second != "http://" + host)
            return respond(s, 403, "Forbidden", "text/plain", "bad origin");

    const Target target = splitTarget(req.path);
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) {
        std::string html;
        if (!readFile((std::filesystem::path(opts.webDir) / "index.html").string(), html))
            return respond(s, 500, "Error", "text/plain", "web/index.html not found in " + opts.webDir);
        return respond(s, 200, "OK", "text/html; charset=utf-8", html);
    }
    if (req.method == "GET" && req.path == "/api/keys") {
        std::string out;
        for (const auto& n : allKeyNames()) out += n + "\n";
        return respond(s, 200, "OK", "text/plain; charset=utf-8", out);
    }
    if (req.method == "GET" && req.path == "/api/platform")
        return respond(s, 200, "OK", "text/plain; charset=utf-8", platform::name());
    if (req.method == "GET" && req.path == "/api/actions") {
        // Built-in actions, one per line: name, description, steps ("ctrl+c, ctrl+v"), default shortcuts, category; tab-separated.
        std::string out;
        if (opts.library)
            for (const auto& name : opts.library->actionOrder) {
                const Steps& steps = opts.library->actions.at(name);
                std::string desc;
                if (auto it = opts.library->actionInfo.find(name); it != opts.library->actionInfo.end()) desc = it->second;
                std::string shortcuts;
                if (auto sc = opts.library->actionShortcuts.find(name); sc != opts.library->actionShortcuts.end())
                    shortcuts = stepsToText(sc->second);
                std::string category;
                if (auto cat = opts.library->actionCategory.find(name); cat != opts.library->actionCategory.end()) category = cat->second;
                out += name + "\t" + desc + "\t" + stepsToText(steps) + "\t" + shortcuts + "\t" + category + "\n";
            }
        return respond(s, 200, "OK", "text/plain; charset=utf-8", out);
    }
    if (req.method == "POST" && target.path == "/api/quit") {  // the GUI's Stop button
        // Same rule as saving: a custom header can't be sent cross-site without a CORS preflight.
        if (req.headers["x-requested-with"] != "keymapper")
            return respond(s, 403, "Forbidden", "text/plain", "missing header");
        respond(s, 200, "OK", "text/plain", "stopping");
        if (opts.onQuit) opts.onQuit();
        return;
    }
    if (req.method == "GET" && req.path == "/api/devices") {
        std::string out = "last\t" + platform::lastKeyboard() + "\n";
        for (const auto& k : platform::listKeyboards()) {
            const bool own = fileExists(deviceConfigPath(opts, k.id));
            out += k.id + "\t" + (own ? "1" : "0") + "\t" + k.name + "\n";
        }
        return respond(s, 200, "OK", "text/plain; charset=utf-8", out);
    }

    if (target.path == "/api/config") {
        const std::string device = queryParam(target.query, "device");
        if (!device.empty() && !validDeviceId(device))
            return respond(s, 400, "Bad Request", "text/plain", "bad device id");
        const std::string file = device.empty() ? opts.configPath : deviceConfigPath(opts, device);

        if (req.method == "GET") {
            std::string text;
            // A keyboard without its own file starts from the default config.
            if (!readFile(file, text) && !device.empty()) readFile(opts.configPath, text);
            return respond(s, 200, "OK", "text/plain; charset=utf-8", text);
        }
        // A custom header can't be sent cross-site without a CORS preflight, which we never grant.
        if (req.headers["x-requested-with"] != "keymapper")
            return respond(s, 403, "Forbidden", "text/plain", "missing header");

        if (req.method == "DELETE" && !device.empty()) {  // back to the default config
            std::error_code ec;
            if (fileExists(file) && !std::filesystem::remove(file, ec))
                return respond(s, 500, "Error", "text/plain", "cannot delete " + file);
            opts.engine->setConfig(device, nullptr);
            return respond(s, 200, "OK", "text/plain", "ok");
        }
        if (req.method == "PUT") {
            auto cfg = std::make_shared<Config>();
            std::vector<std::string> errors;
            if (!parseConfig(req.body, *cfg, errors, opts.library.get())) {
                std::string msg;
                for (const auto& e : errors) msg += e + "\n";
                return respond(s, 400, "Bad Request", "text/plain; charset=utf-8", msg);
            }
            if (!device.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(opts.devicesDir, ec);
            }
            if (!writeFile(file, req.body))
                return respond(s, 500, "Error", "text/plain", "cannot write " + file);
            platform::adoptConfig(device, *cfg);
            opts.engine->setConfig(device, std::move(cfg));
            return respond(s, 200, "OK", "text/plain", "ok");
        }
    }
    // The GUI's scripts and styles: GET /js/app.js, /css/app.css, ... from the web folder.
    if (req.method == "GET" && isSafeStaticPath(target.path)) {
        if (const char* type = contentTypeFor(target.path)) {
            const std::string file = (std::filesystem::path(opts.webDir) / target.path.substr(1)).string();  // path was validated
            std::string body;
            if (readFile(file, body)) return respond(s, 200, "OK", type, body);
        }
    }
    respond(s, 404, "Not Found", "text/plain", "not found");
}

void serve(socket_t listener, ServerOptions opts) {
    for (;;) {
        socket_t c = accept(listener, nullptr, nullptr);
        if (c == kInvalidSocket) continue;
        setReceiveTimeout(c, 5000);
        handle(c, opts);
        closeSocket(c);
    }
}

}  // namespace

bool startServer(const ServerOptions& opts) {
    if (!startNetworking()) return false;

    socket_t listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidSocket) return false;
    allowQuickRestart(listener);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(opts.port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);  // loopback only

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 ||
        listen(listener, 8) != 0) {
        closeSocket(listener);
        return false;
    }
    std::thread(serve, listener, opts).detach();
    return true;
}

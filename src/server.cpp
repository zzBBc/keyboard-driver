#include "server.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

#include "config_loader.h"
#include "hook.h"
#include "key_names.h"

#pragma comment(lib, "ws2_32.lib")

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

bool readRequest(SOCKET s, Request& req) {
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

void respond(SOCKET s, int status, const char* reason, const char* type, const std::string& body) {
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

std::string deviceConfigPath(const ServerOptions& opts, const std::string& id) {
    return opts.devicesDir + "\\" + id + ".txt";
}

bool fileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void handle(SOCKET s, const ServerOptions& opts) {
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
        if (!readFile(opts.webDir + "\\index.html", html))
            return respond(s, 500, "Error", "text/plain", "web/index.html not found in " + opts.webDir);
        return respond(s, 200, "OK", "text/html; charset=utf-8", html);
    }
    if (req.method == "GET" && req.path == "/api/keys") {
        std::string out;
        for (const auto& n : allKeyNames()) out += n + "\n";
        return respond(s, 200, "OK", "text/plain; charset=utf-8", out);
    }
    if (req.method == "GET" && req.path == "/api/devices") {
        std::string out = "last\t" + lastKeyboard() + "\n";
        for (const auto& k : listKeyboards()) {
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
            if (fileExists(file) && !DeleteFileA(file.c_str()))
                return respond(s, 500, "Error", "text/plain", "cannot delete " + file);
            setConfig(device, nullptr);
            return respond(s, 200, "OK", "text/plain", "ok");
        }
        if (req.method == "PUT") {
            auto cfg = std::make_shared<Config>();
            std::vector<std::string> errors;
            if (!parseConfig(req.body, *cfg, errors)) {
                std::string msg;
                for (const auto& e : errors) msg += e + "\n";
                return respond(s, 400, "Bad Request", "text/plain; charset=utf-8", msg);
            }
            if (!device.empty()) CreateDirectoryA(opts.devicesDir.c_str(), nullptr);
            if (!writeFile(file, req.body))
                return respond(s, 500, "Error", "text/plain", "cannot write " + file);
            setConfig(device, std::move(cfg));
            return respond(s, 200, "OK", "text/plain", "ok");
        }
    }
    respond(s, 404, "Not Found", "text/plain", "not found");
}

void serve(SOCKET listener, ServerOptions opts) {
    for (;;) {
        SOCKET c = accept(listener, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        DWORD timeoutMs = 5000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof timeoutMs);
        handle(c, opts);
        closesocket(c);
    }
}

}  // namespace

bool startServer(const ServerOptions& opts) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(opts.port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);  // loopback only

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == SOCKET_ERROR ||
        listen(listener, 8) == SOCKET_ERROR) {
        closesocket(listener);
        return false;
    }
    std::thread(serve, listener, opts).detach();
    return true;
}

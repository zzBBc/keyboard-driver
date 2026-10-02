#include "static_files.h"

#include <cctype>

bool isSafeStaticPath(const std::string& urlPath) {
    if (urlPath.size() < 2 || urlPath.size() > 200 || urlPath[0] != '/') return false;
    for (char c : urlPath)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' || c == '/')) return false;

    size_t start = 1;
    while (start <= urlPath.size()) {
        size_t slash = urlPath.find('/', start);
        const std::string seg = urlPath.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (seg.empty() || seg[0] == '.') return false;  // "//", trailing "/", "..", ".hidden"
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

const char* contentTypeFor(const std::string& path) {
    const auto dot = path.find_last_of('.');
    const auto slash = path.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return nullptr;
    std::string ext = path.substr(dot);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".js") return "text/javascript; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".ico") return "image/x-icon";
    return nullptr;
}

#pragma once
#include <string>

// Rules for the extra files the GUI loads (scripts, styles) from the web folder.

// True for a plain absolute URL path like "/js/app.js": only letters, digits, '_', '-', '.' and '/',
// no empty or dot-leading segments (so no "..", no hidden files), not absurdly long.
// The query string must already be removed.
bool isSafeStaticPath(const std::string& urlPath);

// Content-Type for a servable web asset by extension (.js .css .html .json .svg .png .ico), or null
// for anything else, so only web assets are ever served.
const char* contentTypeFor(const std::string& path);

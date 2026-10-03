// Which files the GUI server may serve.
#include "harness.h"
#include "helpers.h"

#include "static_files.h"

TEST(staticPathsAreSafe) {
    CHECK(isSafeStaticPath("/js/app.js"));
    CHECK(isSafeStaticPath("/css/app.css"));
    CHECK(isSafeStaticPath("/js/keyboard-layout.js"));
    CHECK(!isSafeStaticPath(""));
    CHECK(!isSafeStaticPath("/"));
    CHECK(!isSafeStaticPath("js/app.js"));              // must start with '/'
    CHECK(!isSafeStaticPath("/../secret.txt"));
    CHECK(!isSafeStaticPath("/js/../../secret.txt"));
    CHECK(!isSafeStaticPath("/js/..\\..\\secret.txt"));  // backslashes
    CHECK(!isSafeStaticPath("//server/share/x.js"));
    CHECK(!isSafeStaticPath("/c:/windows/win.ini"));    // drive letters
    CHECK(!isSafeStaticPath("/js/%2e%2e/x.js"));        // encoded dots
    CHECK(!isSafeStaticPath("/.git/config"));            // hidden files
    CHECK(!isSafeStaticPath("/js/app.js?x=1"));          // the query is stripped before this check
    CHECK(!isSafeStaticPath(std::string("/js/") + std::string(300, 'a') + ".js"));  // absurdly long
}

TEST(staticContentTypes) {
    CHECK(std::string(contentTypeFor("/js/app.js")).find("text/javascript") == 0);
    CHECK(std::string(contentTypeFor("/css/app.css")).find("text/css") == 0);
    CHECK(std::string(contentTypeFor("/index.html")).find("text/html") == 0);
    CHECK(std::string(contentTypeFor("/data/x.json")).find("application/json") == 0);
    CHECK(std::string(contentTypeFor("/img/a.svg")) == "image/svg+xml");
    CHECK(contentTypeFor("/mappings.txt") == nullptr);   // only web assets are served
    CHECK(contentTypeFor("/keymapper.exe") == nullptr);
    CHECK(contentTypeFor("/noextension") == nullptr);
}

cmake_minimum_required(VERSION 3.16)
project(keyboard_driver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Portable core: config parsing, key names, chord matching and the engine. No OS calls in here.
add_library(keymapper_core STATIC
    src/core/config_loader.cpp
    src/core/key_names.cpp
    src/core/matcher.cpp
    src/core/engine.cpp
    src/core/static_files.cpp
)
target_include_directories(keymapper_core PUBLIC src/core)

# The program: the core, the local web server, and one platform layer (src/platform/<os>/) that
# intercepts keys and injects the engine's output. platform.h is the interface a layer implements.
add_executable(keymapper
    src/main.cpp
    src/server/server.cpp
)
target_include_directories(keymapper PRIVATE src/server src/platform)
target_link_libraries(keymapper PRIVATE keymapper_core)

if(WIN32)
    target_sources(keymapper PRIVATE src/platform/windows/windows_platform.cpp)
    target_compile_definitions(keymapper PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(keymapper PRIVATE ws2_32 hid shell32)
else()
    message(FATAL_ERROR "There is no platform layer for this OS yet. Add src/platform/<os>/ "
                        "implementing src/platform/platform.h and list it here.")
endif()

# Next to the exe: the GUI page, the built-in actions, and the default config used on first run.
# (mappings.txt itself is never overwritten, since the GUI edits it.)
add_custom_command(TARGET keymapper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/web $<TARGET_FILE_DIR:keymapper>/web
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config/mappings.txt
            $<TARGET_FILE_DIR:keymapper>/mappings.default.txt
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config/actions.txt
            $<TARGET_FILE_DIR:keymapper>/actions.txt)

# Unit tests: build/Release/keymapper_tests.exe (also runnable via `ctest -C Release`).
enable_testing()
add_executable(keymapper_tests
    tests/main.cpp
    tests/config_tests.cpp
    tests/matcher_tests.cpp
    tests/keys_tests.cpp
    tests/engine_tests.cpp
    tests/static_files_tests.cpp
)
target_include_directories(keymapper_tests PRIVATE tests)
target_link_libraries(keymapper_tests PRIVATE keymapper_core)
add_test(NAME keymapper_tests COMMAND keymapper_tests)

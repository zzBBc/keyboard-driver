cmake_minimum_required(VERSION 3.16)
project(keyboard_driver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(keymapper
    src/main.cpp
    src/key_names.cpp
    src/config_loader.cpp
    src/hook.cpp
    src/server.cpp
)
target_include_directories(keymapper PRIVATE src)
target_compile_definitions(keymapper PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(keymapper PRIVATE ws2_32 hid)

# Next to the exe: the GUI page, and the default config used on first run.
# (mappings.txt itself is never overwritten, since the GUI edits it.)
add_custom_command(TARGET keymapper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/web $<TARGET_FILE_DIR:keymapper>/web
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/config/mappings.txt
            $<TARGET_FILE_DIR:keymapper>/mappings.default.txt)

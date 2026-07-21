cmake_minimum_required(VERSION 3.22)
project(compiz_gnome_engine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE tooling / LSP
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options & Flags
option(BUILD_TESTS "Build unit tests" OFF)

# Find Required Packages
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)

# Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
)

# Core Engine Source Files
set(ENGINE_SOURCES
    src/core_cpp/main.cpp
    src/core_cpp/ipc_server.cpp
    src/core_cpp/vulkan_context.cpp
    src/core_cpp/framegraph.cpp
    src/core_cpp/passes/water_compute_pass.cpp
    src/core_cpp/passes/wobbly_compute_pass.cpp
    src/core_cpp/passes/burn_compute_pass.cpp
)

# Core Executable
add_executable(compiz_gnome_engine ${ENGINE_SOURCES})

target_link_libraries(compiz_gnome_engine
    PRIVATE
    Vulkan::Vulkan
    ${WAYLAND_CLIENT_LIBRARIES}
    pthread
)

target_include_directories(compiz_gnome_engine PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core_cpp
)

message(STATUS "compiz-gnome C++20 / Vulkan Engine configured successfully.")

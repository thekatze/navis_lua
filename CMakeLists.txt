cmake_minimum_required(VERSION 3.10)

project(navis-lua)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE) # clangd lsp support

find_package(PkgConfig REQUIRED)

find_package(sol2 CONFIG REQUIRED)
find_package(unofficial-chipmunk CONFIG REQUIRED)
pkg_check_modules(LuaJIT REQUIRED IMPORTED_TARGET luajit)

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_image CONFIG REQUIRED)

add_executable(navis-lua 
    src/engine/IScene.cpp
    src/engine/SceneStack.cpp
    src/engine/AssetManager.cpp
    src/main.cpp
)

target_include_directories(navis-lua PRIVATE src/)

target_link_libraries(navis-lua PRIVATE 
    SDL3::SDL3 
    SDL3_image::SDL3_image-static
    unofficial::chipmunk::chipmunk 
    sol2 
    PkgConfig::LuaJIT)

target_compile_options(navis-lua PRIVATE -O2)

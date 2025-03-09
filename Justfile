set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

setup:
    cmake --preset=default

build:
    cmake --build build/ --target navis-lua

[windows]
clean:
    -rm -Recurse build/

[unix]
clean:
    -rm -rf build/

run: build
    open ./assets/scripting/
    ./build/navis-lua

release: clean
    cmake --preset=default -DCMAKE_BUILD_TYPE=Release
    cmake --build build/


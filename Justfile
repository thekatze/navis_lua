set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

setup:
    cmake --preset=default

build:
    cmake --build build/ --target me-when-lua

[windows]
clean:
    -rm -Recurse build/

[unix]
clean:
    -rm -rf build/

run: build
    ./build/me-when-lua

release: clean
    cmake --preset=default -DCMAKE_BUILD_TYPE=Release
    cmake --build build/


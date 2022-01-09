---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Building Chainblocks

## Windows

### Requirements

* msys2 - https://www.msys2.org/
* rust - gnu toolchain

### Instructions

*We use gcc and clang a lot, MSVC might work but it's uncharted territory.*

Make sure you pull this repository with dependencies `git submodule update --init --recursive`

Make sure to use the gnu toolchain for rust with `rustup default stable-x86_64-pc-windows-gnu`

1. Open mingw prompt (use `mingw64.exe` from msys2 folder), run those commands:
    1. `pacman -Syu --noconfirm` - to do a full update, you might have to reopen the prompt window
    2. `pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld` - to install dependencies
2. Add mingw bin to your system environment PATH variable (the windows one, control panel), in otder to run from powershell, vscode, etc
    * Usually `C:\msys64\mingw64\bin`
3. Open your favorite terminal (could be within visual studio code) pointing to this repo folder
4. Run those commands:
    ```
    mkdir build
    cd build
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
    ninja format; ninja cbl
    ```

#### WASM build

Use a `mingw64.exe` terminal for this.

1. Clone: https://github.com/emscripten-core/emsdk.git

2. Run:
    ```
    ./emsdk install latest
    ./emsdk activate latest
    source emsdk_env.sh
    ```
3. Go back to chainblocks directory and run:
    ```
    mkdir build-wasm
    cd build-wasm
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake ..
    ninja format; ninja rust_blocks; ninja cbl
    ```


--8<-- "includes/license.md"

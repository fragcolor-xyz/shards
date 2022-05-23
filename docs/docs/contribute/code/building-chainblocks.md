---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Building Chainblocks

!!! note
    Before you start, ensure you've [set up your development environment](../getting-started/#development-environment).

This guide will outline the process to build [Chainblocks](https://github.com/fragcolor-xyz/chainblocks) from the sources.

## Windows

### Requirements

!!! note
    We use GCC and Clang a lot; MSVC might work, but it's uncharted territory.

For Windows, ensure your system environment PATH variable includess the MinGW bin location. This value can be set from: Settings > Edit environment variables for your account > User variables for 'user' > Path > Edit. This allows you to run MinGW from Powershell, VS Code, etc. The value of this PATH is usually `C:\msys64\mingw64\bin`.

![Add mingw64 bin to user's PATH](assets/build-cb_acc-env-var.png)

Pull the Chainblocks repository with dependencies using the following command (from any terminal).

```
git submodule update --init --recursive
```

Install and switch to the Rust GNU toolchain with the following rustup commands (from any terminal).

=== "Command"

    ```
    rustup toolchain install nightly
    rustup default nightly-x86_64-pc-windows-gnu
    ```

=== "Output"

    ```
    C:\Users\saten>rustup default nightly-x86_64-pc-windows-gnu
    info: syncing channel updates for 'nightly-x86_64-pc-windows-gnu'
    info: latest update on 2022-01-20, rust version 1.58.1 (db9d1b20b 2022-01-20)
    info: downloading component 'cargo'
    info: downloading component 'clippy'
    info: downloading component 'rust-docs'
     18.8 MiB /  18.8 MiB (100 %)  9.4 MiB/s in  2s ETA:  0s
    .
    .
    .
    info: installing component 'rustfmt'
    info: default toolchain set to 'nightly-x86_64-pc-windows-gnu'
     nightly-x86_64-pc-windows-gnu installed - rust 1.58.1 (db9d1b20b 2022-01-20)

    C:\Users\saten>
    ```

When adding rust targets, ensure they're installed for nightly toolchain. For example to add the target `x86_64-pc-windows-gnu` run the following command.

=== "Command"

    ```
    rustup +nightly target add x86_64-pc-windows-gnu
    ```

=== "Output"

    ```
    info: downloading component 'rust-std' for 'x86_64-pc-windows-gnu'
    info: installing component 'rust-std' for 'x86_64-pc-windows-gnu'
        24.5 MiB /  24.5 MiB (100 %)   6.0 MiB/s in  4s ETA:  0s
    ```

### Update system packages

On Windows you'll need to run the commands in this section in a MingW terminal. To get to this terminal go to the Windows start menu, search for 'MSYS2 MingW' and click the version appropriate for your machine (x86 or x64).

Now run the following command to update your packages:

=== "Command"

    ```
    pacman -Syu --noconfirm
    ```

=== "Output"

    ```
    $ pacman -Syu
    :: Synchronizing package databases...
    mingw32                        805.0 KiB
    mingw32.sig                    438.0   B
    mingw64                        807.9 KiB
    mingw64.sig                    438.0   B
    msys                           289.3 KiB
    msys.sig                       438.0   B
    :: Starting core system upgrade...
    .
    .
    .
    ```

Restart the MingW terminal (if needed) and install the required build dependencies with this command (replace the `w64-x86_64` as appropriate for your target OS/machine).

=== "Command"

    ```
    pacman -Sy --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld wget
    ```

=== "Output"

    ```
    :: Synchronizing package databases...
    warning: base-devel-2022.01-1 is up to date -- skipping
    warning: mingw-w64-x86_64-binutils-2.37-4 is up to date -- skipping
    warning: mingw-w64-x86_64-crt-git-9.0.0.6373.5be8fcd83-1 is up to date -- skipping
    .
    .
    .
    (13/15) upgrading mingw-w64-x86_64-clang           [#####################] 100%
    (14/15) upgrading mingw-w64-x86_64-lld             [#####################] 100%
    (15/15) upgrading wget                             [#####################] 100%
    :: Running post-transaction hooks...
    (1/1) Updating the info directory file...
    ```

### Build & run the project

Continuing in the MingW terminal, navigate to Chainblocks root directory, and run the `bootstrap` script via the following command.

=== "Command"

    ```
    ./bootstrap
    ```

=== "Output"

    ```
    Using CHAINBLOCKS_ROOT=/c/Users/saten/Desktop/code/chainblocks
    /c/Users/saten/Desktop/code/chainblocks/deps /c/Users/saten/Desktop/code/chainblocks
    Setting up dependencies
    /c/Users/saten/Desktop/code/chainblocks
    Setting up tools
    /c/Users/saten/Desktop/code/chainblocks/src/tools /c/Users/saten/Desktop/code/chainblocks
    -- Build spdlog: 1.8.5
    -- Build type: Release
    -- Configuring done
    -- Generating done
    -- Build files have been written to: C:/Users/saten/Desktop/code/chainblocks/src/tools/build
    [1/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/stdout_sinks.cpp.obj
    [2/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/color_sinks.cpp.obj
    [3/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/fmt.cpp.obj
    [4/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/spdlog.cpp.obj
    [5/6] Linking CXX static library deps\spdlog\libspdlog.a
    [6/6] Linking CXX executable bin\bin2c.exe
    /c/Users/saten/Desktop/code/chainblocks
    ```
    
Now you may continue in a normal Windows/VS Code terminal.

Go to Chainblocks root and create a build directory,

```
mkdir build
```

then navigate to it.

```
cd build
```

!!!note
    Running the `bootstrap` script and creating the build folder need only be done once, at set-up time.

Now, run the following command to describe the build,

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
```

and then, run the following command to actually build and link Chainblocks.

```
ninja format; ninja cbl
```

The build ends with a successful linking of the Chainblocks executable (cbl.exe).

 ![Linking Chainblocks cbl.exe](assets/build-cb_use-build-link.png)

*NOTE - When generating the Rust bindings during compilation, the file rust/src/chainblocksc.rs  might be updated automatically. These changes should not be pushed upstream unless you're modifying certain core files for build-target architecture changes (which is very rare). Hence, use the git command `git update-index --skip-worktree rust/src/chainblocksc.rs` to let git ignore changes to this file.*

### Build for Web Assembly

To create a Web Assembly (WASM) build, first clone the Emscripten SDK repo.

```
git clone https://github.com/emscripten-core/emsdk.git
```

Use a `mingw64.exe` terminal to navigate to the emsdk directory,

```
cd emsdk
```

and then do a git pull to get the latest tools from GitHub (not required the first time you clone the git repository).

```
git pull
```

Update the SDK tools to the latest version.

```
emsdk install latest
```

Activate the latest SDK for the current user.

```
emsdk activate latest
```

Activate the PATH/ environment variables for the current terminal session.

```
emsdk_env.bat
```

!! note
    For non-Windows systems, use these commands instead: `./emsdk install latest`, `./emsdk activate latest`, and `source emsdk_env.sh`.

Open a Windows or VS Code terminal and navigate to the Chainblocks directory. Run the following commands in sequence to create and link the Web Assembly build.

```
mkdir build-wasm
```

```
cd build-wasm
```

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake ..
```

```
ninja format; ninja cbl
```

*For more details, refer to the official Emscripten SDK [documentation](https://emscripten.org/docs/getting_started/downloads.html).*


--8<-- "includes/license.md"

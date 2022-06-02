---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Building Shards

This guide explains how to build [Shards](https://github.com/fragcolor-xyz/shards) from the sources, for Windows.

Before you start, ensure you've [set up your development environment](../getting-started/#development-environment).

## Requirements

!!! note
    We use GCC and Clang a lot; MSVC might work, but it's uncharted territory.

For Windows, ensure your system environment PATH variable includess the MinGW bin location. This value can be set from: Settings > Edit environment variables for your account > User variables for 'user' > Path > Edit. This allows you to run MinGW from Powershell, VS Code, etc. The value of this PATH is usually `C:\msys64\mingw64\bin`.

![Add mingw64 bin to user's PATH](assets/build-sh_acc-env-var.png)

You'd normally clone the Shards repository locally (`git clone ...`), checkout the branch you want to work with (`git checkout ...`), and pull the latest changes to your machine (`git pull`).

But since the Shards repository contains [submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) (links to external repositories), you also have to pull the latest changes for these submodules (including nested submodules).

This can be done with the following command (to be run every time you want to build the project).

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

## Update system packages

You should update your system packages frequently and preferably every time you want to build the project.

On Windows you'll need to run these commands in a MinGW terminal. To get to this terminal go to the Windows start menu, search for 'MSYS2 MinGW' and click the version appropriate for your machine (x86 or x64).

Update the Rust packages with `rustup` (works with Windows, Mac, and Linux).

```
rustup update
```

Next, update other packages with `pacman`.

!!! note
    `pacman` comes preinstalled on Linux and as part of `MSYS2` on Windows. For Mac you can get it [here](https://github.com/kladd/pacman-osx) or use the pre-installed package manager, [Homebrew](https://formulae.brew.sh/).

Sync, refresh, and update all local packages that have a newer version available.

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

Restart the MinGW terminal (if needed) and install the required build dependencies with this command (replace the `w64-x86_64` as appropriate for your target OS/machine).

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

## Build & run the project

### Bootstrap the project

Continuing with the MinGW terminal, navigate to Shards root directory, and run the `bootstrap` shell script (to be run only once, when you build the project for the first time).

=== "Windows"

    ```
    ./bootstrap
    ```

=== "Mac/Linux"
    ```
    bash bootstrap
    ```

=== "Output"

    ```
    Using SHARDS_ROOT=/c/Users/saten/Desktop/code/shards
    /c/Users/saten/Desktop/code/shards/deps /c/Users/saten/Desktop/code/shards
    Setting up dependencies
    /c/Users/saten/Desktop/code/shards
    Setting up tools
    /c/Users/saten/Desktop/code/shards/src/tools /c/Users/saten/Desktop/code/shards
    -- Build spdlog: 1.8.5
    -- Build type: Release
    -- Configuring done
    -- Generating done
    -- Build files have been written to: C:/Users/saten/Desktop/code/shards/src/tools/build
    [1/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/stdout_sinks.cpp.obj
    [2/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/color_sinks.cpp.obj
    [3/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/fmt.cpp.obj
    [4/6] Building CXX object deps/spdlog/CMakeFiles/spdlog.dir/src/spdlog.cpp.obj
    [5/6] Linking CXX static library deps\spdlog\libspdlog.a
    [6/6] Linking CXX executable bin\bin2c.exe
    /c/Users/saten/Desktop/code/shards
    ```

### Build the project

Now you may continue in a normal Windows/VS Code terminal.

Go to Shards root and create a build directory (if it doesn't already exist) and navigate to it.

```
mkdir build
cd build
```
You need to run the following two commands every time you want to build the project.

Configure the build with `cmake`,

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..
```

??? note "Release mode build"
    In case you need less verbose script execution logs, build Shards in release mode (instead of the debug mode) by using the command `cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..`

then format the source code and build the target with `ninja`

```
ninja format; ninja shards
```

??? note "Build without formatting"
    Formatting the source is required when raising a PR (for contributing a change). For testing the build locally just use `ninja shards`.

The build ends with a successful linking of the Shards executable (shards.exe).

??? note "Fix build errors"
    If your build fails due to target/file errors navigate out of the build folder, run `cargo clean`, then `cd build`, followed by `ninja clean`, and/or delete the `target` folder and then rebuild. The build can also fail if your software packages or repository submodules (`/shards/deps/`) are out-of-date. To resolve this update the specific software package as given [here](#update-system-packages) and pull the submodules again via `git submodule update --init --recursive`.

??? note
    When generating the Rust bindings during compilation, the file `rust/src/shardsc.rs` might be updated automatically. These changes should not be pushed upstream unless you're modifying certain core files for build-target architecture changes (which is very rare). Hence, use the git command `git update-index --skip-worktree rust/src/shardsc.rs` to let git ignore changes to this file.

### Verify build and run

To verify your build was successful create an empty script file (*.edn) in the `/build` folder, populate it with the **Script code**, and execute the **Run command** from the `/build` folder.

=== "Script code"

    ```
    (defmesh main)
    (defloop test
        (Msg "Hello World"))
    (schedule main test)
    (run main 1 1)
    ```

=== "Run command"

    ```
    ./shards <script-filename.edn>
    ```

=== "Script result"

    ```
    [info] [2022-05-24 06:09:39.293] [T-3196] [logging.cpp::98] [test] Hello World
    ```

If you see `Hello World` printed on your screen (the **Script result**) your build was successful.

You can also configure the `Run Code` button on VS Code (arrow/triangle on the top right-hand corner of the code-editor) to run Shards scripts.

1. Install the VS Code [code-runner](https://marketplace.visualstudio.com/items?itemName=formulahendry.code-runner) extension
2. Locate the `code-runner.executorMap` parameter to open in `settings.json` (VS Code > File > Preferences > Settings > search for code-runner.executorMap > click 'Edit in settings.json')
3. In the `settings.json` file set the value of the code-runner.excutorMap to point to `build\\shards.exe` for `clojure`. You can also add a second entry setting `code-runner.runInTerminal` to `true` if you want the script output displayed in the **Terminal** tab of your terminal (instead of in the **Output** tab, which is the default).
```
    "code-runner.executorMap": {
     	"clojure": "build\\shards.exe"
        "code-runner.runInTerminal": true
    },
```

Now open your Shards script file and click the `Run Code` button. The script will be executed and you should see the script's result in your terminal.

## Build for Web Assembly

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

=== "Windows"

    ```
    emsdk install latest
    ```

=== "Mac/Linux"

    ```
    ./emsdk install latest
    ```

Activate the latest SDK for the current user.

=== "Windows"

    ```
    emsdk activate latest
    ```

=== "Mac/Linux"

    ```
    ./emsdk activate latest
    ```

Activate the PATH/ environment variables for the current terminal session.

=== "Windows"

    ```
    emsdk_env.bat
    ```

=== "Mac/Linux"

    ```
    source emsdk_env.sh
    ```


Open a Windows or VS Code terminal and navigate to the Shards directory. Run the following commands in sequence to create and link the Web Assembly build.

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
ninja format; ninja shards
```

Refer to the official Emscripten SDK [documentation](https://emscripten.org/docs/getting_started/downloads.html) for more details on building for Web Assembly.

## Appendix A - Valid cmake targets

You may also build for targets other than `shards`.

This section gives the complete list of supported `cmake` targets.

### Build Targets

`shards` - Builds shards. the runtime + lisp interpreter.

`shards-dll` - Builds shards as a dynamic library.

`shards-static` - Builds shards as a static library.

`shards-core-shared` - Builds the shards runtime as a dynamic library.

`shards-core-static` - Builds the shards runtime as a static library.

`shards-extra` - Builds the shards-extra library containing the shards inside src/extra.

`cargo-shards-rust` - Builds the rust project inside the rust folder.

### Formatting targets

`format` - Runs clang-format on a list of files defined in cmake/Tidy.cmake.

`tidy` - Runs clang-tidy on a list of files defined in cmake/Tidy.cmake.

### Test targets

These contain tests written in c++ for shards (separate from the edn scripts).

`test_extra` - C++ Tests for extra shards.

`test_gfx` - C++ Tests for graphics/rendering.

`test_runtime` - C++ Tests for the shards runtime.

### Graphics targets

`gfx` - The graphics library.

`gfx-sandbox` - Graphics test environment.

`gfx_imgui` - ImGui backend library.

`gfx_texture_file` - Texture loading utilities.

`cargo-wgpu-native` - Builds the rust project inside src/gfx/wgpu-native.


--8<-- "includes/license.md"

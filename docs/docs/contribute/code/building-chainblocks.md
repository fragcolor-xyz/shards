---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Building Chainblocks

*Before you start, ensure you've [set up your development environment](../getting-started/#development-environment).*

This guide will outline the process to build [Chainblocks](https://github.com/fragcolor-xyz/chainblocks) from the sources.

## Windows

### Requirements

*We use GCC and Clang a lot; MSVC might work, but it's uncharted territory.*

For Windows, ensure your system environment PATH variable includess the MinGW bin location. This value can be set from: Settings > Edit environment variables for your account > User variables for 'user' > Path > Edit. This allows you to run MinGW from Powershell, VS Code, etc. The value of this PATH is usually `C:\msys64\mingw64\bin`.

![Add mingw64 bin to user's PATH](assets/build-cb_acc-env-var.png)

Pull the Chainblocks repository with dependencies using the following command (from any terminal).

```
git submodule update --init --recursive
```

Switch to the Rust GNU toolchain with the following rustup command (from any terminal).

=== "Command"

    ```
    rustup default stable-x86_64-pc-windows-gnu
    ```

=== "Output"

    ```
    C:\Users\saten>rustup default stable-x86_64-pc-windows-gnu
    info: syncing channel updates for 'stable-x86_64-pc-windows-gnu'
    info: latest update on 2022-01-20, rust version 1.58.1 (db9d1b20b 2022-01-20)
    info: downloading component 'cargo'
    info: downloading component 'clippy'
    info: downloading component 'rust-docs'
     18.8 MiB /  18.8 MiB (100 %)  9.4 MiB/s in  2s ETA:  0s
    .
    .
    .
    info: installing component 'rustfmt'
    info: default toolchain set to 'stable-x86_64-pc-windows-gnu'
     stable-x86_64-pc-windows-gnu installed - rust 1.58.1 (db9d1b20b 2022-01-20)
    
    C:\Users\saten>
    ```

### Update system packages

Go to the Windows start menu and search for the 'MSYS2 MSYS' terminal application. Start the terminal.

Run the following command from the MYSYS2 MSYS terminal to update your packages:
   
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

Restart the terminal (if needed) and install the required dependencies with this command:

=== "Command"

    ```
    pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld
    ```

=== "Output"

    ```
    $ pacman -S --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-boost mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld
    warning: base-devel-2022.01-1 is up to date -- skipping
    warning: mingw-w64-x86_64-binutils-2.37-4 is up to date -- skipping
    warning: mingw-w64-x86_64-crt-git-9.0.0.6373.5be8fcd83-1 is up to date -- skipping
    .
    .
    .
    .
    (20/24) installing mingw-w64-x86_64-boost      [#####################] 100%
    (21/24) installing mingw-w64-x86_64-ninja      [#####################] 100%
    (22/24) installing mingw-w64-x86_64-llvm       [#####################] 100%    
    (23/24) installing mingw-w64-x86_64-clang      [#####################] 100%
    (24/24) installing mingw-w64-x86_64-lld        [#####################] 100%
    ```

### Build & run the project

Open a Windows or VS Code terminal and go to the Chainblocks directory.

Now, create a build directory,

```
mkdir build
```

and then navigate to it.

```
cd build
```

Now run the following command to describe the build,

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

*NOTE - For non-Windows systems, use these commands instead: `./emsdk install latest`, `./emsdk activate latest`, and `source emsdk_env.sh`.*

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
ninja format; ninja rust_blocks; ninja cbl
```

*For more details, refer to the official Emscripten SDK [documentation](https://emscripten.org/docs/getting_started/downloads.html).*


--8<-- "includes/license.md"

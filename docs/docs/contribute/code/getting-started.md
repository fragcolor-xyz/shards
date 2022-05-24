---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Getting started

## Stack components

The Fragcolor ecosystem has a few primary components.

- [Chainblocks](https://github.com/fragcolor-xyz/chainblocks) open-scripting programming language
- [Hasten](https://github.com/fragcolor-xyz/hasten) creativity engine (to build apps, games, and open worlds)
- [Clamor](https://github.com/fragcolor-xyz/clamor) protocol and networking stack

To contribute code to any of the above projects, you'll first need to set up a development environment.

## Development environment

### Setup git & GitHub
To install git on your machine (for Windows, Linux, and Mac), refer to the [git installation](../../docs/getting-started/#git) section.

You can download/ clone the code repository via the HTTPS protocol. But you'll need an active GitHub account to fork the repository or contribute to it.

!!! note
    If you're new to git or GitHub read the [Contributing changes](../../docs/contributing-changes/) section. This section covers these topics and applies to both code and documentation contributions.

### Get a code editor
Although you can use any code editor, we recommend Visual Studio Code (VS Code). You can download and install it from [here](https://code.visualstudio.com/download).

Download the appropriate installation file for your platform and follow the on-screen instructions.

### Install & setup C++

If you're using VS Code, you can set up C++ (and the appropriate compiler for your operating system) by referring to Microsoft's official documentation.

- C++ and GCC (MinGW) for [Windows](https://code.visualstudio.com/docs/cpp/config-mingw)
- C++ and GCC for [Linux](https://code.visualstudio.com/docs/cpp/config-linux)
- C++ and Clang/ LLVM for [Mac](https://code.visualstudio.com/docs/cpp/config-clang-mac)
  
??? note "Other code editors"
    For other code editors, refer to the official C++ installation [documentation](https://isocpp.org/get-started).

### Install & setup Rust

Refer to the official Rust [documentation](https://www.rust-lang.org/tools/install) to install Rust on your machine.

After installation, you can set up Rust to work with VS Code for [Windows](https://docs.microsoft.com/en-us/windows/dev-environment/rust/setup#install-rust), [Linux](https://www.nayab.xyz/rust/rust-010-setting-up-rust-vscode-linux), or [Mac](https://levelup.gitconnected.com/rust-with-visual-studio-code-46404befed8), as needed.

Before attempting to build a Rust project, ensure that you've covered the following development dependencies:

- Get latest package information 
  ```
  sudo apt-get update
  ```

- Install the `rust gcc toolchain`
  ```
  sudo apt install build-essential
  ```
  
- Install `clang` 
  ```
  sudo apt install clang
  ```
  
- Install the `rust wasm toolchain`
  ```
  rustup target add wasm32-unknown-unknown --toolchain nightly
  ```

In our projects we use the nightly version of the Rust tools. Since the nightly build is updated every week, you should run the following update command weekly as well:
```
rustup update
```

To switch from the stable build to the nightly build, use:
```
rustup default nightly
```

And to switch back to the stable build, use:
```
rustup default stable
```

??? note "WSL for Windows"
    If you're using Windows you might want to check out [WSL](https://docs.microsoft.com/en-us/windows/wsl/) for a Linux-like development experience. 

### Get VS Code extensions

Here are a few VS Code extensions that we recommend you should check out:

1. [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) by Microsoft
2. [Better TOML](https://marketplace.visualstudio.com/items?itemName=bungcip.better-toml) by bungcip
3. [Calva](https://marketplace.visualstudio.com/items?itemName=betterthantomorrow.calva) by Better than Tomorrow
4. [CMake](https://marketplace.visualstudio.com/items?itemName=twxs.cmake) by twxs
5. [CMake](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) Tools by Microsoft
6. [YAML](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml) by Red Hat

## Build a project from sources

Now you can pick up a project you'd like to contribute to and build it from the sources.

For example, [Building Chainblocks](../building-chainblocks/).

!!! note
    For more details refer to the relevant project's README file.


--8<-- "includes/license.md"
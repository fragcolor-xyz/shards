---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Getting started

## Stack components

The Fragcolor ecosystem has a few primary components.

- [Shards](https://github.com/fragcolor-xyz/shards) open-scripting programming language
- [Claymore](https://github.com/fragcolor-xyz/claymore) visual creativity engine (to build apps, games, and open worlds)
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

??? note "Use `MSYS2 MinGW x64` on Windows"
    If working on Windows, use `MSYS2 MinGW x64` terminal for running any C++ or Rust installation/update commands (*Windows control panel > Search > MSYS2 MinGW x64*). This terminal is installed on Windows as part of the C++ installation process (as mentioned below).

### Install & setup C++

Install and C++ and associated tools as per your operating system from the official links below.

- C++ and GCC (MinGW) for [Windows with VS Code](https://code.visualstudio.com/docs/cpp/config-mingw)
- C++ and GCC for [Linux](https://code.visualstudio.com/docs/cpp/config-linux)
- C++ and Clang/ LLVM for [Mac](https://code.visualstudio.com/docs/cpp/config-clang-mac)

### Install & setup Rust

Refer to the official Rust [documentation](https://www.rust-lang.org/tools/install) to install Rust on your machine, as per your operating system.

After installation, set up Rust to work with VS Code for [Windows](https://docs.microsoft.com/en-us/windows/dev-environment/rust/setup#install-rust), [Linux](https://www.nayab.xyz/rust/rust-010-setting-up-rust-vscode-linux), or [Mac](https://levelup.gitconnected.com/rust-with-visual-studio-code-46404befed8), as needed.

??? note "Add `~/.cargo/bin` to Windows PATH"
    [Rust installation](https://www.rust-lang.org/tools/install), should automatically add `~/.cargo/bin` to your Windows PATH but it's not guaranteed. Hence after installing Rust, verify and update your PATH accordingly: *Windows control panel > Search > "env" > Edit environment variables or your account > Top pane > Path > Edit > Add entry `C:\Users\<your-username>\.cargo\bin` if it doesn't exist > Save*.

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

For example, [Building Shards](../building-shards/).

!!! note
    For more details refer to the relevant project's README file.


--8<-- "includes/license.md"

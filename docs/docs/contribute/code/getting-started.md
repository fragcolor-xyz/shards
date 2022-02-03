---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Getting started

## Stack components
The Fragcolor ecosystem has a few primary components (each in a different state of development right now):

- [Chainblocks](https://github.com/fragcolor-xyz/chainblocks) open-scripting programming language
- [Hasten](https://github.com/fragcolor-xyz/hasten) creativity engine (to build apps, games and open worlds)
- [Clamor](https://github.com/fragcolor-xyz/clamor) protocol and networking stack

To contribute code to any of the above projects you'll need to setup your development environment to work with git, GitHub, a code editor, Rust, & C++.

## Development environment

### Setup git & GitHub
To install git on your machine (for Windows, Linux, & Mac) refer to the [git installation](../../docs/installation/#git) section.

You can download/ clone the code repository via the https protocol but you'll need an active GitHub account if you intend to fork the code and/or contribute to it as well.

*If you're not familiar with git/ GitHub refer to the [Contributing changes](../../docs/making-changes/) section which applies to both code and document contributions.*

### Get a code editor
Although you can use any code editor, we recommend Visual Studio Code (aka VS Code). You can can download and install it from [here](https://code.visualstudio.com/download).

*Download the appropriate installtion file for your platform and follow the on-screen instructions.*

### Install C++

If you're using VS Code you can setup C++ and its compiler (GCC, Clang/LLVM) on your machine, by referring to the official VS Code documentation for the same.

- C++ and GCC (MinGW) on [Windows](https://code.visualstudio.com/docs/cpp/config-mingw)
- C++ and GCC on [Linux](https://code.visualstudio.com/docs/cpp/config-linux)
- C++ and Clang/ LLVM on [Mac](https://code.visualstudio.com/docs/cpp/config-clang-mac)
  
*For other code editors refer to the official C++ installation [documentation](https://isocpp.org/get-started).*

### Install Rust

Refer to the official Rust [documentation](https://www.rust-lang.org/tools/install) to install Rust on your machine.

Once Rust is intalled you can set it up to work with VS Code for [Windows](https://docs.microsoft.com/en-us/windows/dev-environment/rust/setup#install-rust), [Linux](https://www.nayab.xyz/rust/rust-010-setting-up-rust-vscode-linux) or [Mac](https://levelup.gitconnected.com/rust-with-visual-studio-code-46404befed8), as needed.

### VS Code extensions

Here are a few VS Code extensions, worth checking out, that we use these extensively at Fragcolor:

1. [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) by Microsoft
2. [Better TOML](https://marketplace.visualstudio.com/items?itemName=bungcip.better-toml) by bungcip
3. [Calva](https://marketplace.visualstudio.com/items?itemName=betterthantomorrow.calva) by Better than Tomorrow
4. [CMake](https://marketplace.visualstudio.com/items?itemName=twxs.cmake) by twxs
5. [CMake](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) Tools by Microsoft
6. [YAML](https://marketplace.visualstudio.com/items?itemName=redhat.vscode-yaml) by Red Hat

## Building from sources

Now that the development environment has been setup, you can pick up a project you'd like to contribute to and build it from the sources (like [Building Chainblocks](../building-chainblocks/)).

*Also refer to the relevant project's README file.*


--8<-- "includes/license.md"

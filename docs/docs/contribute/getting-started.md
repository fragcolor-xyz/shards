---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Getting Started

!!! note
    This guide assists in setting up the development environment for our projects.

    For an overview, click [here](#overview).

## Setting up the C/C++ Compiler

### macOS

1. Install Xcode from the Mac App Store. This includes the Xcode IDE, Git, and necessary compilers.
2. Open Terminal and install the necessary command line tools with:
    ```bash
    xcode-select --install
    ```
3. Install Homebrew, a package manager for macOS, with the following command:
    ```bash
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    ```
4. Use Homebrew to install additional tools:
    ```bash
    brew install cmake ninja llvm
    ```
5. Verify the installation of each tool by running `cmake --version`, `ninja --version`, and `llvm-config --version`.

### Windows

1. Open Command Prompt.
2. Install Chocolatey by following the instructions at [Chocolatey Installation](https://docs.chocolatey.org/en-us/choco/setup#installing-chocolatey).
3. Install the compiler and build tools:
    ```cmd
    choco install -y cmake --installargs '"ADD_CMAKE_TO_PATH=System"'
    choco install -y ninja
    choco install -y llvm
    refreshenv
    ```

    To install `cmake` for the current user only, replace `System` with `User`.

4. Verify the installation:
    - For `cmake`, `ninja`, and `clang`, run:
        ```cmd
        cmake --version
        ninja --version
        clang --version
        ```
    - If any command returns an error, check the `PATH` environment variable.

### Linux

Install `cmake`, `ninja`, `clang`, and other development dependencies using your distribution's package manager. For Ubuntu:
```bash
sudo apt install build-essential cmake ninja-build clang xorg-dev libdbus-1-dev libssl-dev mesa-utils
```

## Rust

Install Rust from [Rust Installation](https://www.rust-lang.org/tools/install).

### Common Instructions for Windows, Linux, and macOS

1. Verify the Rust installation with `cargo --version`.
2. Install the nightly version of Rust if required by your project:
    ```bash
    rustup toolchain install nightly
    rustup +nightly target add <target-architecture>
    ```
    Replace `<target-architecture>` with your system's architecture (e.g., `x86_64-pc-windows-msvc` for Windows, `x86_64-unknown-linux-gnu` for Linux).
3. For web browser support, run:
    ```bash
    rustup +nightly target add wasm32-unknown-unknown
    ```
4. Regularly update Rust using `rustup update`.

### Using a specific nightly version if needed

Before building the project, ensure that you are using the correct version of the Rust toolchain as specified in the project's `rust.version` file. This file is located in the root directory of the project if a specific version is required.

1. Navigate to the project's root directory.
2. Install the specific Rust toolchain version required by the project:
    ```bash
    cat rust.version | xargs rustup toolchain install
    ```
3. Set the installed toolchain as the default for the current project:
    ```bash
    cat rust.version | xargs rustup override set
    ```

Now, proceed with the project-specific build instructions.

## Git & GitHub

Git is a distributed version control system essential for managing project codebases.

### macOS

If not already installed with Xcode, install Git using Homebrew:
```bash
brew install git
```

### Windows

Use Chocolatey for installation:
```cmd
choco install -y git
```

### Linux

Install Git using:
```bash
sudo apt install git
```

## Code Editor

We recommend Visual Studio Code (VS Code) for this tutorial.

Download and install VS Code from [VS Code Download](https://code.visualstudio.com/download)

### VS Code Extensions

Install the following extensions to enhance your development experience:

1. C/C++
2. rust-analyzer
3. CodeLLDB
4. Better TOML
6. CMake Tools
7. YAML
8. [Fragcolor Shards](https://marketplace.visualstudio.com/items?itemName=fragcolor.shards) - Direct link to the Fragcolor Shards extension on the VS Code Marketplace.

## Overview

Here is a quick overview of the setup process for Windows and Linux. Refer to the respective sections above for detailed instructions.

### macOS

1. Install Xcode and command line tools.
2. Install Homebrew.
3. Install `cmake`, `ninja`, and `llvm` using Homebrew.
4. Install Rust and set up the nightly build.
5. Install the Rust Web Assembly toolchain.
6. Regularly update Rust.
7. Install Git using Homebrew.
8. Install VS Code and the recommended extensions.

### Windows

1. Install Chocolatey.
2. Install `cmake`, `ninja`, and `llvm` with Chocolatey.
3. Install Rust and set up the nightly build.
4. Install the Rust Web Assembly toolchain.
5. Regularly update Rust.
6. Install Git and a Git GUI Client.
7. Install VS Code and the recommended extensions.

### Linux

1. Install development dependencies including `cmake`, `ninja`, and `clang`.
2. Install Rust and set up the nightly build.
3. Install the Rust Web Assembly toolchain.
4. Regularly update Rust.
5. Install Git.
6. Install VS Code and the recommended extensions.

--8<-- "includes/license.md"

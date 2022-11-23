---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

<!-- TODO: Add guides for Mac and Linux. -->

# Getting Started

!!! note
    This guide is for new users unfamiliar with the installation process. 

    Click [here](#overview) to skip the tutorial and jump to the overview.

Learn how to set up the development environment to start working with our projects!

## Code Editor ##

A code editor allows you to work with code more easily. We will be using Visual Studio Code (VS Code) for this tutorial. You can download it [here](https://code.visualstudio.com/download), and install it with the default settings.

!!! note
    You are free to use any code editor of your choice, although you will have to complete steps of this tutorial through alternate means.

## Setting up the GCC Compiler ##

A compiler translates human-readable code into machine code. We will be setting up the GCC compiler so that our code written in the human-readable C++ language can be understood by the computers!

First, [download MSYS2](https://www.msys2.org/) and install it with the default settings. MSYS2 helps to set up MinGW-w64, which uses the GCC compiler to create Windows programs.

We will now install the Mingw-w64 toolset which includes GCC, and other build tools that will be employed later. When the MinGW terminal pops up, enter the following command:

```
pacman -Sy --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld wget
```


Next, we will set the Mingw-w64 path to the environment variables. This will allow the terminal to search the Mingw-w64 directory for executables to run without requiring the user to specify the full address of the .exe each time.

Search for “Edit environment variables for your account.” in the Window’s search bar and select the option that appears.

!!! help
    You can also access the Environment Variables window from "Control Panel → User Accounts → User Accounts → Change my environment variables".

Double-click on “Path” under “User variables”.

![Double-click on the “Path” user variable.](assets/set-environment-variable-mingw64-path.png)

In the “Edit environment variable” window, select “New” and add `C:\msys64\mingw64\bin` to the empty field that appears.
```
C:\msys64\mingw64\bin
```

![Add the mingw64 bin directory to the environment variables.](assets/set-environment-variable-mingw64-new.png)

Click the “OK” button twice to close the windows and save the changes.

## Rust ##

Rust is a programming language that we will be using. Install it [here](https://www.rust-lang.org/tools/install). 

Open up the Command Prompt. You can use the Windows Search Bar to find the Command Prompt application.

![Search for the Command Prompt in the Window’s search bar.](assets/search-command-prompt.png)

!!! tip
    You can check if Rust is properly installed by using `cargo --version` in the Command Prompt. 

    If a version number is printed, your installation was successful!
        ```
        cargo --version
        ```

We will be using the nightly version of Rust which is updated more frequently compared to the stable and beta versions. Install it with the following commands:
    ```
    rustup toolchain install nightly
    ```
    ```
    rustup +nightly target add x86_64-pc-windows-gnu
    ```
	```
    rustup default nightly-x86_64-pc-windows-gnu
	```

To add support for building to web browsers, input the following command:
	```
    rustup +nightly target add wasm32-unknown-unknown
	```

Use the `rustup update` command to update your installation. Since we are using the nightly release, you are encouraged to update often.
	```
    rustup update
	```
 
<!-- For Linux / WSL

Only 2 commands are required to set up the Rust nightly build:

rustup install nightly
rustup default nightly

-->

##  Git & GitHub ##

Git is a system used for managing and tracking changes in your code. It is also known as a Version Control System (VCS). It makes it easier for collaborators to work together, and allows you to access our projects too!

Install Git [here](https://git-scm.com/download). The installation settings can be left unchanged.

Although Git is powerful, it can be daunting to use on its own. Instead, we use GitHub, a service which employs Git’s version control with its own to make project collaboration much easier. 

We recommend that you use a [Git GUI Client](https://git-scm.com/downloads/guis) to facilitate your work by pruning off the manual input of code and making the version control process more visual.

We will be using [GitHub Desktop](https://desktop.github.com/) for the tutorials. You can install it with the default settings, and create a GitHub account when prompted if you do not have one.

!!! note
    Even though we recommend GitHub Desktop, feel free to use any GUI client of your choice!


## VS Code Extensions ##

Launch Visual Studio Code and open the Extensions view (Ctrl+Shift+X). We will be installing a few extensions to facilitate our VS Code experience.

![Installing the C/C++ extension from the Extensions Market](assets/install-c-cpp-extension.png)

Search for and install the following extensions:

1. C/C++

2. rust-analyzer

3. CodeLLDB 

4. Better TOML

5. Calva

6. CMake

7. CMake Tools

8. YAML

## Overview ##

1. Install the [VS Code Editor](https://code.visualstudio.com/download).

2. Install [MSYS2](https://www.msys2.org/).

3. Install the Mingw-w64 toolset with the MinGW terminal.
```
pacman -Sy --needed --noconfirm base-devel mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-lld wget
```

4. Add the Mingw-w64 folder path to the user’s system path.
```
C:\msys64\mingw64\bin
```

5. Install [Rust](https://www.rust-lang.org/tools/install).

6. Install and set up the Rust nightly build.
```
rustup toolchain install nightly
```
```
rustup +nightly target add x86_64-pc-windows-gnu
```
```
rustup default nightly-x86_64-pc-windows-gnu
```


7. Install the Rust Web Assembly toolchain.
```
rustup +nightly target add wasm32-unknown-unknown
```

8. Update Rust.
```
rustup update
```

9. Install [Git](https://git-scm.com/download).

10. Install [GitHub for Desktop](https://desktop.github.com/) or any other [Git GUI Client](https://git-scm.com/downloads/guis).

11. Install [Extensions for Visual Studio Code](#vs-code-extensions).


--8<-- "includes/license.md"

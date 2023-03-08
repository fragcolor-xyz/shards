---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Installing rust

Instal rustup from the official web site: [https://rustup.rs/](https://rustup.rs/) and follow the instructions there.

## Installing the toolchain

Install the toolchain required for building shards, the nightly toolchain is required.

After installing rusup, you can run the command `rustup toolchain list` in a console/terminal to see which toolchains are installed.

If the required version is not listed you can install it with `rustup toolchain install <toolchain name>`

Check below for platform-specific examples

### Windows

Install the `x86_64-pc-windows-msvc` target for windows systems. Note the `-msvc` suffix here is required when used alongside the recommended LLVM C/C++ toolchain.
Use `rustup +nightly target add x86_64-pc-windows-msvc`.

### MacOS / Linux

Installing `rustup toolchain install nightly` should be enough to install the nightly toolchain that works alongside the system's C/C++ compiler.

### Android

Asuming you're building for arm64 android devices, install the `aarch64-linux-android` target using `rustup +nightly target add aarch64-linux-android`.


--8<-- "includes/license.md"

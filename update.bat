@echo off

:: Sync submodules recursively
git submodule sync --recursive

:: Update and initialize submodules recursively
git submodule update --init --recursive

:: Read rust.version file and install the specified Rust toolchain
for /f "delims=" %%x in (rust.version) do rustup toolchain install %%x

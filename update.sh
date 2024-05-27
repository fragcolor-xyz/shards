git submodule sync --recursive
git submodule update --init --recursive
cat rust.version | xargs rustup toolchain install
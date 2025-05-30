if [ -d ".git" ]; then
    echo "Git repository detected, updating submodules..."
    git submodule sync --recursive
    git submodule update --init --recursive
else
    echo "No git repository, skipping submodule updates"
fi

# Rust toolchain should probably always run
cat rust.version | xargs rustup toolchain install --component rust-src

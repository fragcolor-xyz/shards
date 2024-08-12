# --- BASE STAGE ---
FROM ubuntu:latest

# Update and install dependencies
RUN apt-get update -y && \
    apt-get install -y gnupg git cmake pkg-config libssl-dev build-essential clang libclang-dev curl protobuf-compiler wget ninja-build xorg-dev libdbus-1-dev lcov mesa-utils lld linux-tools-generic valgrind ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Setting up Rust environment
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --no-modify-path --default-toolchain nightly && \
    chmod -R a+w $RUSTUP_HOME $CARGO_HOME && \
    rustup default nightly && \
    rustup update nightly && \
    rustup update stable && \
    rustup toolchain install nightly && \
    rustup default nightly

# Confirming Rust installation
RUN rustup --version && cargo --version && rustc --version

# Cloning shards repository with submodules
RUN git clone --recursive https://github.com/fragcolor-xyz/shards.git /shards

# Update CA certificates
RUN update-ca-certificates

# Updating shards with specified version
RUN cd /shards && \
    git fetch --all && \
    git submodule update --init --recursive && \
    ./bootstrap && \
    cat rust.version | xargs rustup toolchain install

# Cmake build shards RelWithDebinfo and using ninja
RUN cd /shards && \
    mkdir -p build && \
    cd build && \
    cmake -G Ninja -DUSE_VALGRIND=1 -DSHARDS_WITH_EXTRA_SHARDS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo .. && \
    ninja shards
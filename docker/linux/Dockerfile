# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2019 Fragcolor Pte. Ltd.

FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update
RUN apt-get -y install tzdata
RUN apt-get -y install build-essential git cmake wget clang ninja-build valgrind xorg-dev libdbus-1-dev libssl-dev lcov ca-certificates

ENV RUSTUP_HOME=/usr/local/rustup
ENV CARGO_HOME=/usr/local/cargo
ENV PATH=/usr/local/cargo/bin:$PATH

RUN set -eux; \
    url="https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-gnu/rustup-init"; \
    wget "$url"; \
    chmod +x rustup-init; \
    ./rustup-init -y --no-modify-path --default-toolchain nightly; \
    rm rustup-init; \
    chmod -R a+w $RUSTUP_HOME $CARGO_HOME; \
    rustup --version; \
    cargo --version; \
    rustc --version;

RUN rustup toolchain install nightly

ARG USER_ID=1000

RUN useradd -ms /bin/bash -u ${USER_ID} shards

USER shards

WORKDIR /home/shards

COPY --chown=shards ./ /home/shards/repo

WORKDIR /home/shards/repo

ENV HOME=/home/shards

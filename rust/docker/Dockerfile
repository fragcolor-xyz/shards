# SPDX-License-Identifier: BSD-3-Clause
# Copyright © 2020 Fragcolor Pte. Ltd.

FROM archlinux/base

RUN pacman -Syu --noconfirm && pacman -S --noconfirm base-devel git curl cmake wget clang ninja boost
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:$PATH

RUN set -eux; \
    \
    url="https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-gnu/rustup-init"; \
    wget "$url"; \
    chmod +x rustup-init; \
    ./rustup-init -y --no-modify-path --default-toolchain stable; \
    rm rustup-init; \
    chmod -R a+w $RUSTUP_HOME $CARGO_HOME; \
    rustup --version; \
    cargo --version; \
    rustc --version;

RUN rustup toolchain install nightly

ARG USER_ID=1000

RUN useradd -ms /bin/bash -u ${USER_ID} tester

USER tester

WORKDIR /home/tester

COPY --chown=tester ./ /home/tester/test

WORKDIR /home/tester/test

ENV HOME=/home/tester

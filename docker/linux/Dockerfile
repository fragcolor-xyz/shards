FROM archlinux/base

RUN pacman -Syu --noconfirm && pacman -S --noconfirm base-devel git cmake wget clang ninja boost gdb lld

ARG USER_ID=1000

RUN useradd -ms /bin/bash -u ${USER_ID} tester

USER tester

WORKDIR /home/tester

COPY --chown=tester ./ /home/tester/test

WORKDIR /home/tester/test

ENV HOME=/home/tester

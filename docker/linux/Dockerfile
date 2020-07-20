FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update
RUN apt-get -y install tzdata
RUN apt-get -y install build-essential git cmake wget clang ninja-build libboost-all-dev valgrind xorg-dev libdbus-1-dev libssl-dev

ARG USER_ID=1000

RUN useradd -ms /bin/bash -u ${USER_ID} tester

USER tester

WORKDIR /home/tester

COPY --chown=tester ./ /home/tester/test

WORKDIR /home/tester/test

ENV HOME=/home/tester

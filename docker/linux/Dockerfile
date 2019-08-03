FROM ubuntu:18.04

RUN apt-get update && apt-get install -y build-essential git wget curl clang libboost-all-dev cmake ninja

ARG USER_ID
ARG GROUP_ID

RUN useradd -ms /bin/bash -u ${USER_ID} tester

USER tester

WORKDIR /home/tester

RUN wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh

RUN chmod +x Miniconda3-latest-Linux-x86_64.sh && \
./Miniconda3-latest-Linux-x86_64.sh -b -p /home/tester/miniconda3

ENV PATH=$PATH:/home/tester/miniconda3/bin

RUN conda install -c fragcolor nim=0.20.99.1

COPY --chown=tester ./ /home/tester/test

WORKDIR /home/tester/test

ENV HOME=/home/tester

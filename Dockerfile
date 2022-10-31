FROM ubuntu:20.04

ARG TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt update && apt upgrade -y && apt install -y build-essential software-properties-common zip unzip
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt install -y gcc-11 g++-11 libasound2-dev

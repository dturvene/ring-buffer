# Container for ringbuffer demo
# See $GK/docker for base configs

# FROM debian:10.0 as base
FROM debian:buster as base

# set environment
ENV LANG C.UTF-8
ENV TERM=linux
ENV DEBIAN_FRONTEND=noninteractive

ARG PYTHON=python3
ARG PIP=pip3

ARG USER=user1
ARG GROUP=user1
# These must be identical to the host user for shared volumes
ARG USERID=1000
ARG GROUPID=1000

RUN apt-get update --fix-missing && \
    apt-get install -y \
    	    apt-utils \
	    ${PYTHON} \
	    ${PYTHON}-pip \
	    cmake \
	    time \
	    pandoc \
	    build-essential \
    	    sudo

# upgrade for current python3 support
RUN ${PIP} --no-cache-dir install --upgrade \
    pip \
    setuptools
RUN ln -s $(which ${PYTHON}) /usr/local/bin/python

# set up non-root user with sudo rights
RUN adduser --disabled-password --gecos '' ${USER} && \
    adduser ${USER} sudo && \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# enter user
USER ${USER}

# update USER environment
ENV PS1='\u:\!> '
ENV PATH=/home/${USER}/.local/bin:$PATH

# Update user meson package to most recent
RUN ${PIP} install -U meson



FROM fedora:44

ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN dnf update -y && \
    dnf install -y \
    gcc-c++ \
    gcc \
    clang\
    clang-tools-extra \
    cmake \
    ninja-build \
    ccache \
    lldb \
    git \
    shadow-utils \
    sudo \
    which \
    gdb \
    make \
    libstdc++-devel \
    libstdc++-static \
    && dnf clean all

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

ENV CCACHE_DIR=/home/$USERNAME/.cache/ccache
RUN mkdir -p $CCACHE_DIR && chown -R $USERNAME:$USERNAME /home/$USERNAME/.cache

ENV CC=gcc
ENV CXX=g++

USER $USERNAME
WORKDIR /workspace
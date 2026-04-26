FROM fedora:rawhide

ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN dnf update -y && \
    dnf install -y \
    # GCC — primary compiler (trunk snapshot, GCC 16)
    gcc \
    gcc-c++ \
    # GCC stdlib — provides bits/std.cc for `import std;` with GCC
    libstdc++-devel \
    libstdc++-static \
    # Clang toolchain — clangd for IntelliSense ONLY, not for building
    clang \
    clang-tools-extra \
    # Build system
    cmake \
    ninja-build \
    make \
    # Performance
    ccache \
    # Debuggers
    gdb \
    lldb \
    # Utilities
    git \
    python3 \
    shadow-utils \
    sudo \
    which \
    && dnf clean all

# Non-root user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# ccache
ENV CCACHE_DIR=/home/$USERNAME/.cache/ccache
RUN mkdir -p $CCACHE_DIR && chown -R $USERNAME:$USERNAME /home/$USERNAME/.cache

# GCC as primary compiler
ENV CC=gcc
ENV CXX=g++

USER $USERNAME
WORKDIR /workspace
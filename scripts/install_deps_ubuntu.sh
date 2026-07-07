#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
   echo "ERR: This script must be run as root (sudo)" >&2
   exit 1
fi

echo "INFO: Updating package lists..."
apt-get update
apt-get install -y software-properties-common
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update

echo "INFO: Installing development tools and dependencies..."
apt-get install -y \
    build-essential \
    gcc-15 \
    g++-15 \
    git \
    cmake \
    clang \
    ninja-build \
    meson \
    pkg-config \
    python3 \
    python3-pip \
    python3-pyelftools \
    libnuma-dev \
    clang-format \
    libpcap-dev

echo "INFO: Requirements installation complete."

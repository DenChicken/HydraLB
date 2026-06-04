#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
   echo "ERR: This script must be run as root (sudo)" >&2
   exit 1
fi

echo "INFO: Updating package lists..."
apt-get update

echo "INFO: Installing development tools and dependencies..."
apt-get install -y \
    build-essential \
    git \
    cmake \
    clang \
    ninja-build \
    meson \
    pkg-config \
    python3 \
    python3-pip \
    python3-pyelftools \
    libnuma-dev

echo "INFO: Requirements installation complete."

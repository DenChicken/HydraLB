#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DPDK_SRC="${PROJECT_ROOT}/externals/dpdk"
DPDK_BUILD="${PROJECT_ROOT}/build/externals/dpdk-build"
DPDK_INSTALL="${PROJECT_ROOT}/build/externals/dpdk-install"

if [[ ! -d "${DPDK_SRC}/lib" ]]; then
    echo "ERR: DPDK submodule not initialized. Run: git submodule update --init --recursive" >&2
    exit 1
fi

cd "${DPDK_SRC}"

export CC="gcc-15"
export CXX="g++-15"

echo "INFO: Configuring DPDK 24.11..."
meson setup "${DPDK_BUILD}" \
    --prefix="${DPDK_INSTALL}" \
    --buildtype=release \
    -Dplatform=native \
    -Denable_drivers="net,common,net/pcap" \
    -Denable_docs=false \
    -Dtests=false

echo "INFO: Building DPDK..."
ninja -C "${DPDK_BUILD}" -j"$(nproc)"

echo "INFO: Installing DPDK to ${DPDK_INSTALL}..."
ninja -C "${DPDK_BUILD}" install

echo "INFO: DPDK build complete → ${DPDK_INSTALL}"

#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_ROOT}"

echo "INFO: Running clang-format..."

find . -type f \
    -not -path "*/.git/*" \
    -not -path "*/.build/*" \
    -not -path "*/build/*" \
    -not -path "*/cmake-build-debug/*" \
    -not -path "*/cmake-build-release/*" \
    -not -path "*/node_modules/*" \
    -not -path "*/.qtcreator/*" \
    -not -path "*/externals/*" \
    \( \
        -name "*.cpp" -o \
        -name "*.cc" -o \
        -name "*.cxx" -o \
        -name "*.h" -o \
        -name "*.hpp" -o \
        -name "*.hxx" -o \
        -name "*.cppm" -o \
        -name "*.ixx" \
    \) -exec clang-format -i {} +

echo "INFO: Formatting complete."

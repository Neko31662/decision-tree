#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${ROOT_DIR}"

echo "[1/3] Installing EMP toolkit dependencies..."
wget -q https://raw.githubusercontent.com/emp-toolkit/emp-readme/master/scripts/install.py
python3 install.py --tool
rm -f install.py

echo "[2/3] Initializing submodules and building third-party FSS..."
git submodule update --init --recursive
FSS_DIR="${ROOT_DIR}/third_party/fss"
cmake -S "${FSS_DIR}" -B "${FSS_DIR}/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "${FSS_DIR}/build" -j"$(nproc)"

echo "[3/3] Building this project..."
cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "${ROOT_DIR}/build" -j"$(nproc)"

echo "Build completed."

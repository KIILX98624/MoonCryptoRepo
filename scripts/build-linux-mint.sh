#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-linux-mint"

if [[ ! -f /etc/os-release ]]; then
    echo "Cannot identify the Linux distribution (/etc/os-release missing)." >&2
    exit 1
fi

. /etc/os-release
ID_LIKE_LOWER="$(printf '%s' "${ID_LIKE:-}" | tr '[:upper:]' '[:lower:]')"

if [[ "${ID:-}" != "linuxmint" && "${ID:-}" != "ubuntu" && "${ID:-}" != "debian" && "${ID_LIKE_LOWER}" != *ubuntu* && "${ID_LIKE_LOWER}" != *debian* ]]; then
    echo "Warning: this helper targets Linux Mint and its Ubuntu/Debian base." >&2
    echo "Detected distribution: ${PRETTY_NAME:-unknown}" >&2
fi

for tool in cmake g++ make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool" >&2
        echo "Install the build requirements with:" >&2
        echo "  sudo apt update && sudo apt install -y build-essential cmake libssl-dev" >&2
        exit 1
    fi
done

CMAKE_VERSION="$(cmake --version | head -n1)"
echo "==> Configuring MonC for Linux Mint"
echo "    $CMAKE_VERSION"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENSSL_ROOT_DIR=/usr

echo "==> Building MonC"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo
echo "Build completed."
echo "Executable: $BUILD_DIR/monc"
echo
echo "Try:"
echo "  \"$BUILD_DIR/monc\" --help"

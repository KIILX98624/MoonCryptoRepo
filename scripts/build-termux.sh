#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

if [[ -z "${PREFIX:-}" || "$PREFIX" != /data/data/com.termux/files/usr* ]]; then
    echo "This helper is intended to run inside Termux."
    echo "Install the dependencies manually with:"
    echo "  pkg install git clang cmake make pkg-config openssl"
    exit 1
fi

for tool in cmake clang++ make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing required tool: $tool"
        echo "Install dependencies with:"
        echo "  pkg install clang cmake make pkg-config openssl"
        exit 1
    fi
done

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${MONC_BUILD_DIR:-$ROOT_DIR/build-termux}"

echo "==> Configuring MonC for Termux"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DOPENSSL_ROOT_DIR="$PREFIX"

echo "==> Building MonC"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo
echo "Build complete: $BUILD_DIR/monc"
echo "Try:"
echo "  \"$BUILD_DIR/monc\" --help"

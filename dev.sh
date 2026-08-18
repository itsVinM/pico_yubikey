#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

IMAGE="pico-yubikey-dev"
BUILD_DIR="build-host"
FW_BUILD_DIR="build-fw"
UF2="$FW_BUILD_DIR/src/pico_yubikey.uf2"
RESC="renode/pico_yubikey.resc"

# ── helpers ───────────────────────────────────────────────────────────────

ensure_docker() {
    if ! docker info >/dev/null 2>&1; then
        echo "Docker engine not running — starting OrbStack..."
        open -a OrbStack 2>/dev/null || open -a "Docker Desktop" 2>/dev/null || true
        for i in $(seq 1 30); do
            docker info >/dev/null 2>&1 && break
            sleep 1
        done
        docker info >/dev/null 2>&1 || { echo "Docker failed to start"; exit 1; }
    fi
}

usage() {
    cat <<EOF
pico-yubikey dev helper

Usage: ./dev.sh <command> [args]

Commands:
  test          Build and run host tests in Docker (default)
  shell         Interactive shell inside Docker container
  build         Build firmware (requires Pico SDK)
  flash         Flash .uf2 to Pico via picotool (USB BOOTSEL)
  renode        Launch Renode with the firmware ELF
  renode-test   Run Renode headless test (CI-friendly)
  lint          Run clang-tidy on core headers
  <anything>    Pass through to docker run

Examples:
  ./dev.sh test                          # run tests in Docker
  ./dev.sh build                         # build firmware
  ./dev.sh flash                         # flash connected Pico
  ./dev.sh renode                        # interactive Renode session
  ./dev.sh shell                         # drop into container shell
  ./dev.sh g++ -std=c++20 -Iinclude ...  # custom compile in container
EOF
}

# ── commands ──────────────────────────────────────────────────────────────

cmd_test() {
    ensure_docker
    docker build -t "$IMAGE" .
    echo "=== Building and running tests in Docker ==="
    docker run --rm -v "$(pwd)":/app "$IMAGE" \
        bash -cm "cmake -B $BUILD_DIR && cmake --build $BUILD_DIR && ctest --test-dir $BUILD_DIR --output-on-failure"
}

cmd_shell() {
    ensure_docker
    docker build -t "$IMAGE" .
    docker run --rm -it -v "$(pwd)":/app "$IMAGE" bash
}

cmd_build() {
    if [ ! -d "${PICO_SDK_PATH:-$HOME/pico-sdk}/external" ]; then
        echo "ERROR: Pico SDK not found at PICO_SDK_PATH=${PICO_SDK_PATH:-$HOME/pico-sdk}"
        echo "Install: git clone https://github.com/raspberrypi/pico-sdk.git --recurse-submodules"
        exit 1
    fi
    cmake -B "$FW_BUILD_DIR" -DPICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico-sdk}"
    cmake --build "$FW_BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    echo ""
    echo "Firmware: $UF2"
    echo "Flash: ./dev.sh flash"
}

cmd_flash() {
    if [ ! -f "$UF2" ]; then
        echo "ERROR: $UF2 not found — run './dev.sh build' first"
        exit 1
    fi
    if ! command -v picotool &>/dev/null; then
        echo "ERROR: picotool not found"
        echo "Install: brew install picotool   (macOS)"
        echo "         git clone https://github.com/raspberrypi/picotool && cd picotool && mkdir build && cd build && cmake .. && make"
        exit 1
    fi
    echo "Waiting for Pico in BOOTSEL mode..."
    echo "(Hold BOOTSEL while plugging in the USB cable)"
    picotool load -f "$UF2"
    picotool reboot
    echo "Flashed and rebooted."
}

cmd_renode() {
    if ! command -v renode &>/dev/null; then
        echo "ERROR: renode not found"
        echo "Install: brew install renode   (macOS)"
        echo "         https://github.com/renode/renode/releases  (Linux)"
        exit 1
    fi
    if [ ! -f "$FW_BUILD_DIR/src/pico_yubikey.elf" ]; then
        echo "ERROR: ELF not found — run './dev.sh build' first"
        exit 1
    fi
    renode "$RESC"
}

cmd_renode_test() {
    if ! command -v renode-test &>/dev/null; then
        echo "ERROR: renode-test not found (install renode)"
        exit 1
    fi
    if [ ! -f "$FW_BUILD_DIR/src/pico_yubikey.elf" ]; then
        echo "ERROR: ELF not found — run './dev.sh build' first"
        exit 1
    fi
    renode-test renode/pico_yubikey_test.robot
}

cmd_lint() {
    ensure_docker
    docker build -t "$IMAGE" .
    docker run --rm -v "$(pwd)":/app "$IMAGE" \
        bash -c "apt-get update -qq && apt-get install -y -qq clang-tidy >/dev/null 2>&1; \
                 find include/core -name '*.hpp' | xargs clang-tidy -std=c++20 -- -Iinclude"
}

# ── dispatch ──────────────────────────────────────────────────────────────

case "${1:-test}" in
    test)        cmd_test ;;
    shell|bash)  cmd_shell ;;
    build)       cmd_build ;;
    flash)       cmd_flash ;;
    renode)      cmd_renode ;;
    renode-test) cmd_renode_test ;;
    lint)        cmd_lint ;;
    -h|--help)   usage ;;
    *)           docker run --rm -it -v "$(pwd)":/app "$IMAGE" "$@" ;;
esac

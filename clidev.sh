#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

IMAGE="pico-yubikey-dev"
BUILD_DIR="build-host"
FW_BUILD_DIR="build-fw"
UF2="$FW_BUILD_DIR/src/pico_yubikey.uf2"
QEMU="tools/qemu-rp2040/build/qemu-system-arm"


# ========== helpers ============

# todo!() add self check for OS
ensure_docker(){
    if ! docker info >/dev/null 2>&1; then
        echo "Docker engine not running - starting Orbstack..."
        open -a OrbStack 2>/dev/null || open -a "Docker Desktop" 2>/dev/null || true
        for idx in {1..30};do
            docker info >/dev/null 2>&1 && break
            sleep 1
        done
    fi
}

usage() {
    cat <<EOF
pico-yubikey dev helper

Usage: ./clidev.sh <command> [args]

Commands:
  test           Build and run host tests in Docker (default)
  shell          Interactive shell inside Docker container
  build          Build firmware (requires Pico SDK)
  flash          Flash .uf2 to Pico via picotool (USB BOOTSEL)
  qemu           Run the firmware in the local QEMU RP2040 fork
  lint           Run clang-tidy on core headers
  <anything>     Pass through to docker run

Examples:
  ./clidev.sh test                          # run tests in Docker
  ./clidev.sh build                         # build firmware
  ./clidev.sh flash                         # flash connected Pico
  ./clidev.sh qemu                          # boot firmware in QEMU
  ./clidev.sh shell                         # drop into container shell
  ./clidev.sh g++ -std=c++20 -Iinclude ...  # custom compile in container
EOF
}

# ======= COMMANDS ======

cmd_test(){
    ensure_docker
    docker build -t "$IMAGE" .
    echo "=== Building and running tests in Docker ==="
    docker run --rm -v "$(pwd)":/app "$IMAGE" \
        bash -cm "cmake -B $BUILD_DIR && cmake --build $BUILD_DIR && ctest --test-dir $BUILD_DIR --output-on-failure"
}

cmd_shell(){
    ensure_docker
    docker build -t "$IMAGE" .
    docker run --rm -it -v "$(pwd)":/app "$IMAGE" bash
}

cmd_build(){
    if [ ! -d "${PICO_SDK_PATH:-$HOME/pico-sdk}/external" ]; then
        echo "ERROR: Pico SDK not found at PICO_SDK_PATH=${PICO_SDK_PATH:-$HOME/pico-sdk}"
        echo "Install: git clone https://github.com/raspberrypi/pico-sdk.git --recurse-submodules"
        exit 1
    fi
    cmake -B "$FW_BUILD_DIR" -DPICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico-sdk}"
    cmake --build "$FW_BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
    echo ""
    echo "Firmware: $UF2"
    echo "Flash: ./clidev.sh flash"
}

cmd_flash(){
    if [ ! -f "$UF2" ]; then
        echo "ERROR: $UF2 not found — run './clidev.sh build' first"
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

cmd_qemu(){
    if [ ! -x "$QEMU" ]; then
        echo "ERROR: $QEMU not found"
        echo "Build it: see tools/qemu-rp2040 (configure+build the RP2040 fork)"
        exit 1
    fi
    if [ ! -f "$FW_BUILD_DIR/src/pico_yubikey.elf" ]; then
        echo "ERROR: ELF not found — run './clidev.sh build' first"
        exit 1
    fi
    exec "$QEMU" -M raspi-pico -kernel "$FW_BUILD_DIR/src/pico_yubikey.elf" -nographic -serial mon:stdio
}

cmd_robot_test() {
    if ! python3 -c "import robot" &>/dev/null; then
        echo "ERROR: Robot Framework not installed"
        echo "Install: pip3 install robotframework"
        exit 1
    fi
    if [ ! -f "$FW_BUILD_DIR/src/pico_yubikey.elf" ]; then
        echo "ERROR: ELF not found — run './clidev.sh build' first"
        exit 1
    fi
    if [ ! -x "$QEMU" ]; then
        echo "ERROR: $QEMU not found — build tools/qemu-rp2040 first"
        exit 1
    fi
    python3 -m robot --outputdir build/robot tests/robot
}

cmd_lint() {
    ensure_docker
    docker build -t "$IMAGE" .
    docker run --rm -v "$(pwd)":/app "$IMAGE" \
        bash -c "apt-get update -qq && apt-get install -y -qq clang-tidy >/dev/null 2>&1; \
                 find include/core -name '*.hpp' | xargs clang-tidy -std=c++20 -- -Iinclude"
}

# ===== Dispatch ======
case "${1:-test}" in
    test)           cmd_test ;;
    shell|bash)     cmd_shell ;;
    build)          cmd_build ;;
    flash)          cmd_flash ;;
    qemu)           cmd_qemu ;;
    robot-test)     cmd_robot_test ;;
    lint)           cmd_lint ;;    h|help)      usage ;;
    *)              docker run --rm -it -v "$(pwd)":/app "$IMAGE" "$@" ;;
esac
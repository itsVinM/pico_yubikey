#!/usr/bin/env bash
set -euo pipefail

IMAGE="pico-yubikey-dev"

docker build -t "$IMAGE" .
docker run --rm -it -v "$(pwd)":/app "$IMAGE" "$@"

# Build test inside container
# ./dev.sh g++ -std=c++20 -I include -I tests tests/test_otp.cpp src/core/sha1.cpp && ./a.out

# OR build inside shell
# ./dev.sh bash
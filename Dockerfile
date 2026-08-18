FROM ubuntu:24.04

# ---------- packages ----------
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ cmake make pkg-config \
        python3 python3-pip \
        clang-tidy \
        git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

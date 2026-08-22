# ---------- host-test / firmware toolchain ----------
FROM ubuntu:24.04 AS dev
RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ cmake make pkg-config \
        python3 python3-pip python3-venv \
        clang-tidy git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app

# ---------- QEMU RP2040 fork (built from pinned commit) ----------
FROM ubuntu:24.04 AS qemu-build
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential meson ninja-build pkg-config python3 python3-venv \
        libglib2.0-dev libpixman-1-dev git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/2xs/qemu-rp2040-pico.git /src \
    && cd /src && git checkout cff0d47
WORKDIR /src/build
RUN ../configure --target-list=arm-softmmu --disable-docs --disable-slirp \
        --disable-gtk --disable-sdl --disable-vnc --disable-opengl \
        --disable-capstone --disable-zstd --disable-libusb --disable-werror \
    && make -j"$(nproc)"

# ---------- final image = toolchain + qemu + robot framework ----------
FROM dev
RUN apt-get update && apt-get install -y --no-install-recommends \
        libglib2.0-0t64 libpixman-1-0 zlib1g \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --break-system-packages robotframework
COPY --from=qemu-build /src/build/qemu-system-arm /usr/local/bin/qemu-system-arm
ENV PICO_YUBIKEY_QEMU=/usr/local/bin/qemu-system-arm
WORKDIR /app

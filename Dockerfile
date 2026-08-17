FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

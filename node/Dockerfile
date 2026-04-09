# syntax=docker/dockerfile:1
#
# PHANTOM node build
# Builds liboqs (ML-DSA-65 / ML-KEM-768) + phantom_node from source.
# Produces a minimal Debian image with no build tools in the final stage.

# ── Build stage ─────────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libsqlite3-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Build liboqs (ML-DSA-65 + ML-KEM-768)
RUN git clone --depth 1 https://github.com/open-quantum-safe/liboqs.git /tmp/liboqs \
    && cmake -S /tmp/liboqs -B /tmp/liboqs/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DOQS_BUILD_ONLY_LIB=ON \
        -DOQS_DIST_BUILD=ON \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build /tmp/liboqs/build --parallel \
    && cmake --install /tmp/liboqs/build \
    && rm -rf /tmp/liboqs

# Build phantom_node and phantom_mesh_node
COPY phantom_node.cpp /src/phantom_node.cpp
COPY phantom_mesh_node.cpp /src/phantom_mesh_node.cpp

RUN g++ -std=c++20 -O2 -o /usr/local/bin/phantom_node /src/phantom_node.cpp \
        -I/usr/local/include -L/usr/local/lib \
        /usr/local/lib/liboqs.a -lsqlite3 -lssl -lcrypto \
    && strip /usr/local/bin/phantom_node

RUN g++ -std=c++20 -O2 -o /usr/local/bin/phantom_mesh_node /src/phantom_mesh_node.cpp \
        -I/usr/local/include -L/usr/local/lib \
        /usr/local/lib/liboqs.a -lsqlite3 -lssl -lcrypto -lpthread \
    && strip /usr/local/bin/phantom_mesh_node

# ── Runtime stage ────────────────────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        libsqlite3-0 libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/bin/phantom_node      /usr/local/bin/phantom_node
COPY --from=builder /usr/local/bin/phantom_mesh_node /usr/local/bin/phantom_mesh_node

WORKDIR /data
VOLUME ["/data"]

ENTRYPOINT ["phantom_mesh_node"]

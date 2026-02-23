# Stage 1: Build voltad (Rust bridge daemon)
FROM rust:1.85-bookworm AS voltad-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    libzmq3-dev \
    protobuf-compiler \
    pkg-config \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Cache dependency build: copy manifests first
COPY bridge/Cargo.toml bridge/build.rs ./
COPY bridge/proto/ proto/
RUN mkdir src && echo "fn main(){}" > src/main.rs \
    && cargo build --release 2>/dev/null; exit 0

# Build with real source
COPY bridge/src/ src/
RUN touch src/main.rs && cargo build --release

# Stage 2: Runtime
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV RENODE_VERSION=1.15.2

# Install system dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    wget \
    ca-certificates \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    gdb-multiarch \
    socat \
    make \
    libzmq5 \
    && rm -rf /var/lib/apt/lists/*

# Install Renode portable
RUN wget -q "https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode-${RENODE_VERSION}.linux-portable.tar.gz" \
    -O /tmp/renode.tar.gz \
    && mkdir -p /opt/renode \
    && tar -xzf /tmp/renode.tar.gz -C /opt/renode --strip-components=1 \
    && rm /tmp/renode.tar.gz \
    && ln -s /opt/renode/renode /usr/local/bin/renode \
    && ln -s /opt/renode/renode-test /usr/local/bin/renode-test

# Copy voltad binary
COPY --from=voltad-builder /build/target/release/voltad /usr/local/bin/voltad

# Create volta directories
RUN mkdir -p /opt/volta/{platforms,firmware,peripherals}

# Expose ports: Renode telnet, GDB, ZMQ PUB, gRPC
EXPOSE 1234 3333 5555 50051

COPY containers/mcu/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /opt/volta
ENTRYPOINT ["/entrypoint.sh"]

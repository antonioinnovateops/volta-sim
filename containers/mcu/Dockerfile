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
    && rm -rf /var/lib/apt/lists/*

# Install Renode portable
RUN wget -q "https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode-${RENODE_VERSION}.linux-portable.tar.gz" \
    -O /tmp/renode.tar.gz \
    && mkdir -p /opt/renode \
    && tar -xzf /tmp/renode.tar.gz -C /opt/renode --strip-components=1 \
    && rm /tmp/renode.tar.gz \
    && ln -s /opt/renode/renode /usr/local/bin/renode \
    && ln -s /opt/renode/renode-test /usr/local/bin/renode-test

# Create volta directories
RUN mkdir -p /opt/volta/{platforms,firmware,peripherals}

# Expose Renode telnet monitor and GDB server
EXPOSE 1234 3333

COPY containers/mcu/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /opt/volta
ENTRYPOINT ["/entrypoint.sh"]

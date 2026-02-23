# Volta-Sim UE5 Pixel Streaming Container
# Multi-stage build: source -> engine -> project -> runtime
#
# Build stages separately for caching:
#   docker build --target ue5-engine -t volta-ue5-engine:5.4 ...   (~2-4 hours, one-time)
#   docker build -t volta-ue5 ...                                   (~minutes for project)

ARG UE5_BRANCH=5.4

# ==============================================================================
# Stage 1: Clone UE5 source (requires GitHub token with EpicGames org access)
# ==============================================================================
FROM ubuntu:22.04 AS ue5-source

ARG GITHUB_TOKEN
ARG UE5_BRANCH

RUN apt-get update && apt-get install -y --no-install-recommends \
    git ca-certificates curl \
    libicu70 libicu-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch ${UE5_BRANCH} \
    https://${GITHUB_TOKEN}@github.com/EpicGames/UnrealEngine.git /ue5

WORKDIR /ue5
RUN ./Setup.sh && ./GenerateProjectFiles.sh

# ==============================================================================
# Stage 2: Build UE5 engine (heavy, cache this layer)
# ==============================================================================
FROM ue5-source AS ue5-engine

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential mono-devel clang dos2unix \
    libssl-dev libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ue5
RUN make

# ==============================================================================
# Stage 3: Build VoltaSim project
# ==============================================================================
FROM ue5-engine AS volta-project-builder

# Copy project into UE5 tree
COPY engine/ /ue5/VoltaSim/

WORKDIR /ue5
RUN ./Engine/Build/BatchFiles/RunUAT.sh BuildCookRun \
    -project=/ue5/VoltaSim/VoltaSim.uproject \
    -noP4 -platform=Linux -clientconfig=Shipping \
    -cook -allmaps -build -stage -pak -archive \
    -archivedirectory=/opt/volta/packaged \
    -unrealexe=/ue5/Engine/Binaries/Linux/UnrealEditor-Cmd

# ==============================================================================
# Stage 4: Runtime (minimal image with packaged game)
# ==============================================================================
FROM nvidia/cuda:12.2.2-runtime-ubuntu22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=graphics,compute,utility

# System packages
RUN apt-get update && apt-get install -y --no-install-recommends \
    xvfb vulkan-tools mesa-vulkan-drivers libvulkan1 \
    curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Node.js for signaling server
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

# Copy packaged game from builder
COPY --from=volta-project-builder /opt/volta/packaged /opt/volta/game

# Copy Pixel Streaming signaling server from UE5 source
COPY --from=volta-project-builder \
    /ue5/Engine/Source/Programs/PixelStreaming/WebServers/SignallingWebServer \
    /opt/volta/signaling

WORKDIR /opt/volta/signaling
RUN npm install

# Copy config and entrypoint
COPY containers/ue5/pixel_streaming.json /opt/volta/signaling/config.json
COPY containers/ue5/entrypoint.sh /opt/volta/entrypoint.sh
RUN chmod +x /opt/volta/entrypoint.sh

EXPOSE 8888 8889

ENTRYPOINT ["/opt/volta/entrypoint.sh"]

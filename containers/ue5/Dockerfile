# Volta-Sim UE5 Pixel Streaming Runtime Container
#
# This runs a PRE-PACKAGED UE5 Linux build. It does NOT build UE5 from source.
#
# Workflow:
#   1. Install UE5 on your dev machine (Epic Games Launcher or from source)
#   2. Open engine/VoltaSim.uproject in the editor, build for Linux Shipping
#   3. Copy the packaged output to packaged-game/LinuxServer/
#   4. docker-compose -f docker-compose.yml -f docker-compose.ue5.yml up
#
# Or use the helper: make ue5-package (builds on host) then make ue5-up

FROM adamrehn/ue4-runtime:22.04-cudagl12

ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=graphics,compute,utility,video
ENV DEBIAN_FRONTEND=noninteractive

# Vulkan ICD + display dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libvulkan1 vulkan-tools mesa-vulkan-drivers \
    libegl1 libgl1 libgles2 \
    xvfb x11-utils \
    socat curl \
    && rm -rf /var/lib/apt/lists/*

# NVIDIA Vulkan ICD
RUN mkdir -p /etc/vulkan/icd.d && \
    echo '{"file_format_version":"1.0.0","ICD":{"library_path":"libGLX_nvidia.so.0","api_version":"1.3"}}' \
    > /etc/vulkan/icd.d/nvidia_icd.json

WORKDIR /opt/volta

# Copy pre-packaged game (built on dev machine, NOT in Docker)
# This directory should contain the output of RunUAT BuildCookRun
COPY packaged-game/ /opt/volta/game/

# Copy entrypoint
COPY containers/ue5/entrypoint.sh /opt/volta/entrypoint.sh
RUN chmod +x /opt/volta/entrypoint.sh

# SHM for reading Volta peripheral state
VOLUME ["/dev/shm"]

EXPOSE 8888

ENTRYPOINT ["/opt/volta/entrypoint.sh"]

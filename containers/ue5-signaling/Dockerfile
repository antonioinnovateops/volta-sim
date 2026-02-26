# Pixel Streaming Signaling Server (UE5.4 — plain JS cirrus.js)
# Lightweight Node.js container — no GPU needed

FROM node:20-slim

RUN apt-get update && apt-get install -y --no-install-recommends git ca-certificates && \
    git clone --depth 1 --branch UE5.4 \
        https://github.com/EpicGamesExt/PixelStreamingInfrastructure.git /tmp/ps && \
    cp -r /tmp/ps/SignallingWebServer /opt/signaling && \
    cp -r /tmp/ps/Frontend /opt/signaling/Frontend && \
    rm -rf /tmp/ps && \
    apt-get purge -y git && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/signaling

RUN npm install --production

EXPOSE 80 8889 8890

CMD ["node", "cirrus.js", \
     "--HttpPort=80", \
     "--StreamerPort=8889", \
     "--SFUPort=8890", \
     "--publicIp=localhost"]

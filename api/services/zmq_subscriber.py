"""ZMQ subscriber that forwards voltad events to WebSocket clients."""

import asyncio
import json
import threading
import zmq
import msgpack


class ZmqSubscriber:
    def __init__(self, address: str, ws_manager):
        self._address = address
        self._ws_manager = ws_manager
        self._thread = None
        self._running = False

    def start(self):
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)

    def _run(self):
        ctx = zmq.Context()
        sock = ctx.socket(zmq.SUB)
        sock.connect(self._address)
        sock.setsockopt_string(zmq.SUBSCRIBE, "")
        sock.setsockopt(zmq.RCVTIMEO, 1000)

        while self._running:
            try:
                frames = sock.recv_multipart()
                if len(frames) >= 2:
                    topic = frames[0].decode("utf-8", errors="replace")
                    payload = frames[1]
                    # voltad publishes MessagePack (rmp_serde::to_vec)
                    data = msgpack.unpackb(payload, raw=False) if payload else {}
                    event = {"topic": topic, "data": data}
                    self._ws_manager.broadcast_sync(json.dumps(event))
            except zmq.Again:
                continue
            except Exception:
                continue

        sock.close()
        ctx.term()

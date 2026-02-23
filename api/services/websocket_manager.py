"""WebSocket connection manager for real-time event broadcasting."""

import asyncio
from fastapi import WebSocket


class WebSocketManager:
    def __init__(self):
        self._connections: list[WebSocket] = []
        self._loop: asyncio.AbstractEventLoop | None = None
        self._pending: list[str] = []

    async def connect(self, ws: WebSocket):
        await ws.accept()
        self._connections.append(ws)
        if self._loop is None:
            self._loop = asyncio.get_event_loop()

    def disconnect(self, ws: WebSocket):
        if ws in self._connections:
            self._connections.remove(ws)

    async def broadcast(self, message: str):
        dead = []
        for ws in self._connections:
            try:
                await ws.send_text(message)
            except Exception:
                dead.append(ws)
        for ws in dead:
            self.disconnect(ws)

    def broadcast_sync(self, message: str):
        """Thread-safe broadcast from ZMQ subscriber thread."""
        if self._loop and self._connections:
            asyncio.run_coroutine_threadsafe(self.broadcast(message), self._loop)

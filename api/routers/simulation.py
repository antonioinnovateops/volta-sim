"""Simulation control endpoints."""

from fastapi import APIRouter, Request, WebSocket, WebSocketDisconnect
from pydantic import BaseModel

router = APIRouter()


class StartBody(BaseModel):
    time_scale: float = 1.0
    sync_mode: str = "lockstep"


class StepBody(BaseModel):
    cycles: int = 1000


class CommandBody(BaseModel):
    command: str


@router.post("/start")
async def start(request: Request, body: StartBody | None = None):
    result = await request.app.state.voltad.start_simulation()
    return {**result, "time_scale": body.time_scale if body else 1.0}


@router.post("/stop")
async def stop(request: Request):
    return await request.app.state.voltad.stop_simulation()


@router.get("/status")
async def status(request: Request):
    return await request.app.state.voltad.get_status()


@router.post("/command")
async def run_command(request: Request, body: CommandBody):
    return await request.app.state.voltad.run_command(body.command)


@router.websocket("/ws/events")
async def websocket_events(ws: WebSocket, request: Request = None):
    manager = ws.app.state.ws_manager
    await manager.connect(ws)
    try:
        while True:
            # Keep connection alive; client can send ping
            await ws.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(ws)

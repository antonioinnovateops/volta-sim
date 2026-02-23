"""Volta REST API — wraps voltad gRPC + ZMQ into HTTP/WebSocket endpoints."""

import os
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from api.routers import simulation, firmware, gpio, uart, pwm, adc
from api.services.voltad_client import VoltadClient
from api.services.zmq_subscriber import ZmqSubscriber
from api.services.websocket_manager import WebSocketManager


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    grpc_addr = os.environ.get("VOLTA_MCU_GRPC", "volta-mcu:50051")
    zmq_addr = os.environ.get("VOLTA_ZMQ_SUB", "tcp://volta-mcu:5555")

    app.state.voltad = VoltadClient(grpc_addr)
    app.state.ws_manager = WebSocketManager()
    app.state.zmq_sub = ZmqSubscriber(zmq_addr, app.state.ws_manager)

    await app.state.voltad.connect()
    app.state.zmq_sub.start()

    yield

    # Shutdown
    app.state.zmq_sub.stop()
    await app.state.voltad.close()


app = FastAPI(
    title="Volta API",
    version="0.1.0",
    description="REST API for Volta-Sim hardware-in-the-loop simulation",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(simulation.router, prefix="/api/v1/simulation", tags=["Simulation"])
app.include_router(firmware.router, prefix="/api/v1/firmware", tags=["Firmware"])
app.include_router(gpio.router, prefix="/api/v1/gpio", tags=["GPIO"])
app.include_router(uart.router, prefix="/api/v1/uart", tags=["UART"])
app.include_router(pwm.router, prefix="/api/v1/pwm", tags=["PWM"])
app.include_router(adc.router, prefix="/api/v1/adc", tags=["ADC"])


@app.get("/health")
async def health():
    return {"status": "ok"}

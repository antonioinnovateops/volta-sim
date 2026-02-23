"""gRPC client for voltad bridge daemon."""

import grpc
from api.services import volta_pb2, volta_pb2_grpc


class VoltadClient:
    def __init__(self, address: str):
        self._address = address
        self._channel = None
        self._stub = None

    async def connect(self):
        self._channel = grpc.aio.insecure_channel(self._address)
        self._stub = volta_pb2_grpc.VoltaControlStub(self._channel)

    async def close(self):
        if self._channel:
            await self._channel.close()

    async def flash_firmware(self, elf_path: str) -> dict:
        resp = await self._stub.FlashFirmware(
            volta_pb2.FlashRequest(elf_path=elf_path)
        )
        return {"success": resp.success, "message": resp.message}

    async def start_simulation(self) -> dict:
        resp = await self._stub.StartSimulation(volta_pb2.StartRequest())
        return {"state": resp.state, "message": resp.message}

    async def stop_simulation(self) -> dict:
        resp = await self._stub.StopSimulation(volta_pb2.StopRequest())
        return {"state": resp.state, "message": resp.message}

    async def get_status(self) -> dict:
        resp = await self._stub.GetStatus(volta_pb2.StatusRequest())
        return {"state": resp.state, "message": resp.message}

    async def run_command(self, command: str) -> dict:
        resp = await self._stub.RunCommand(
            volta_pb2.CommandRequest(command=command)
        )
        return {"success": resp.success, "output": resp.output}

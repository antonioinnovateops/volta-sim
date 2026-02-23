"""GPIO state endpoints — reads directly from SHM."""

import mmap
import os
import struct

from fastapi import APIRouter

router = APIRouter()

SHM_PATH = os.environ.get("VOLTA_SHM_PATH", "/dev/shm/volta_peripheral_state")
SHM_SIZE = 65536
GPIO_OFFSET = 0x0040
GPIO_PORT_SIZE = 16
GPIO_PORT_COUNT = 16


def _read_gpio_state() -> dict:
    """Read all GPIO port states from SHM."""
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ) as mm:
            # Validate magic
            magic = struct.unpack_from("<I", mm, 0)[0]
            if magic != 0x564F4C54:
                os.close(fd)
                return {"error": "SHM not initialized (bad magic)"}

            ports = {}
            for port_idx in range(GPIO_PORT_COUNT):
                offset = GPIO_OFFSET + port_idx * GPIO_PORT_SIZE
                output_state = struct.unpack_from("<H", mm, offset)[0]
                input_state = struct.unpack_from("<H", mm, offset + 2)[0]

                if output_state == 0 and input_state == 0:
                    continue

                port_name = chr(ord("A") + port_idx)
                pins = []
                for pin in range(16):
                    out = bool(output_state & (1 << pin))
                    inp = bool(input_state & (1 << pin))
                    if out or inp:
                        pins.append({
                            "pin": pin,
                            "output": "HIGH" if out else "LOW",
                            "input": "HIGH" if inp else "LOW",
                        })
                if pins:
                    ports[port_name] = {"pins": pins}

        os.close(fd)
        return {"ports": ports}
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}


@router.get("")
async def get_gpio():
    return _read_gpio_state()


@router.get("/{port}")
async def get_gpio_port(port: str):
    state = _read_gpio_state()
    port = port.upper()
    if "error" in state:
        return state
    if port in state.get("ports", {}):
        return {port: state["ports"][port]}
    return {port: {"pins": []}}

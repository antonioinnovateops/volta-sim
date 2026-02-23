"""UART buffer endpoints — reads ring buffer from SHM."""

import mmap
import os
import struct

from fastapi import APIRouter

router = APIRouter()

SHM_PATH = os.environ.get("VOLTA_SHM_PATH", "/dev/shm/volta_peripheral_state")
SHM_SIZE = 65536
UART_OFFSET = 0x0140
UART_CHANNEL_SIZE = 1024
UART_HEADER_SIZE = 8
UART_BUF_SIZE = 1016


def _read_uart_buffer(channel: int) -> dict:
    """Read UART ring buffer from SHM."""
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ) as mm:
            ch_offset = UART_OFFSET + channel * UART_CHANNEL_SIZE
            write_head = struct.unpack_from("<I", mm, ch_offset)[0]

            buf_start = ch_offset + UART_HEADER_SIZE
            buf_len = min(write_head, UART_BUF_SIZE)

            if write_head <= UART_BUF_SIZE:
                data = bytes(mm[buf_start : buf_start + buf_len])
            else:
                # Ring buffer wrapped — read from write_head position to end, then start
                pos = write_head % UART_BUF_SIZE
                data = bytes(mm[buf_start + pos : buf_start + UART_BUF_SIZE])
                data += bytes(mm[buf_start : buf_start + pos])

        os.close(fd)
        return {
            "channel": channel,
            "write_head": write_head,
            "data": data.decode("utf-8", errors="replace"),
            "data_hex": data.hex(),
            "length": len(data),
        }
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}


@router.get("/{channel}/buffer")
async def get_uart_buffer(channel: int):
    if channel < 0 or channel > 7:
        return {"error": "Channel must be 0-7"}
    return _read_uart_buffer(channel)

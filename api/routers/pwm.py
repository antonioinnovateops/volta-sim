"""PWM state endpoints — reads directly from SHM."""

import mmap
import os
import struct

from fastapi import APIRouter

router = APIRouter()

SHM_PATH = os.environ.get("VOLTA_SHM_PATH", "/dev/shm/volta_peripheral_state")
SHM_SIZE = 65536
PWM_OFFSET = 0x2140
PWM_CHANNEL_SIZE = 16
PWM_CHANNEL_COUNT = 16


def _read_pwm_channel(mm, channel: int) -> dict:
    """Read a single PWM channel from mmap'd SHM."""
    offset = PWM_OFFSET + channel * PWM_CHANNEL_SIZE
    duty_cycle = struct.unpack_from("<f", mm, offset + 0)[0]
    frequency = struct.unpack_from("<I", mm, offset + 4)[0]
    enabled = struct.unpack_from("<B", mm, offset + 8)[0] != 0
    polarity = struct.unpack_from("<B", mm, offset + 9)[0]
    period_us = struct.unpack_from("<I", mm, offset + 12)[0]

    return {
        "channel": channel,
        "duty_cycle": round(duty_cycle, 4),
        "frequency": frequency,
        "enabled": enabled,
        "polarity": polarity,
        "period_us": period_us,
    }


def _read_all_pwm() -> dict:
    """Read all PWM channel states from SHM."""
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ) as mm:
            magic = struct.unpack_from("<I", mm, 0)[0]
            if magic != 0x564F4C54:
                os.close(fd)
                return {"error": "SHM not initialized (bad magic)"}

            channels = []
            for ch in range(PWM_CHANNEL_COUNT):
                state = _read_pwm_channel(mm, ch)
                if state["enabled"] or state["frequency"] > 0:
                    channels.append(state)

        os.close(fd)
        return {"channels": channels}
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}


@router.get("")
async def get_pwm():
    return _read_all_pwm()


@router.get("/{channel}")
async def get_pwm_channel(channel: int):
    if channel < 0 or channel >= PWM_CHANNEL_COUNT:
        return {"error": f"Invalid channel: {channel} (must be 0-{PWM_CHANNEL_COUNT-1})"}
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ) as mm:
            magic = struct.unpack_from("<I", mm, 0)[0]
            if magic != 0x564F4C54:
                os.close(fd)
                return {"error": "SHM not initialized (bad magic)"}
            result = _read_pwm_channel(mm, channel)
        os.close(fd)
        return result
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}

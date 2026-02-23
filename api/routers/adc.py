"""ADC state endpoints — reads/writes directly from/to SHM."""

import mmap
import os
import struct

from fastapi import APIRouter
from pydantic import BaseModel

router = APIRouter()

SHM_PATH = os.environ.get("VOLTA_SHM_PATH", "/dev/shm/volta_peripheral_state")
SHM_SIZE = 65536
SEQ_OFFSET = 8
ADC_OFFSET = 0x2240
ADC_CHANNEL_SIZE = 16
ADC_CHANNEL_COUNT = 16

VREF = 3.3  # Reference voltage


class AdcInjectRequest(BaseModel):
    voltage: float


def _read_adc_channel(mm, channel: int) -> dict:
    """Read a single ADC channel from mmap'd SHM."""
    offset = ADC_OFFSET + channel * ADC_CHANNEL_SIZE
    raw_value = struct.unpack_from("<H", mm, offset + 0)[0]
    voltage = struct.unpack_from("<f", mm, offset + 2)[0]
    enabled = struct.unpack_from("<B", mm, offset + 6)[0] != 0
    sample_rate = struct.unpack_from("<I", mm, offset + 8)[0]

    return {
        "channel": channel,
        "raw_value": raw_value,
        "voltage": round(voltage, 4),
        "enabled": enabled,
        "sample_rate": sample_rate,
    }


def _read_all_adc() -> dict:
    """Read all ADC channel states from SHM."""
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ) as mm:
            magic = struct.unpack_from("<I", mm, 0)[0]
            if magic != 0x564F4C54:
                os.close(fd)
                return {"error": "SHM not initialized (bad magic)"}

            channels = []
            for ch in range(ADC_CHANNEL_COUNT):
                state = _read_adc_channel(mm, ch)
                if state["enabled"] or state["raw_value"] > 0:
                    channels.append(state)

        os.close(fd)
        return {"channels": channels}
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}


@router.get("")
async def get_adc():
    return _read_all_adc()


@router.get("/{channel}")
async def get_adc_channel(channel: int):
    if channel < 0 or channel >= ADC_CHANNEL_COUNT:
        return {"error": f"Invalid channel: {channel} (must be 0-{ADC_CHANNEL_COUNT-1})"}
    try:
        fd = os.open(SHM_PATH, os.O_RDONLY)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_READ) as mm:
            magic = struct.unpack_from("<I", mm, 0)[0]
            if magic != 0x564F4C54:
                os.close(fd)
                return {"error": "SHM not initialized (bad magic)"}
            result = _read_adc_channel(mm, channel)
        os.close(fd)
        return result
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}


@router.post("/{channel}/inject")
async def inject_adc(channel: int, body: AdcInjectRequest):
    """Inject a voltage value into an ADC channel via SHM."""
    if channel < 0 or channel >= ADC_CHANNEL_COUNT:
        return {"error": f"Invalid channel: {channel} (must be 0-{ADC_CHANNEL_COUNT-1})"}

    voltage = max(0.0, min(body.voltage, VREF))
    raw_value = int((voltage / VREF) * 4095)

    try:
        fd = os.open(SHM_PATH, os.O_RDWR)
        with mmap.mmap(fd, SHM_SIZE, access=mmap.ACCESS_WRITE) as mm:
            magic = struct.unpack_from("<I", mm, 0)[0]
            if magic != 0x564F4C54:
                os.close(fd)
                return {"error": "SHM not initialized (bad magic)"}

            offset = ADC_OFFSET + channel * ADC_CHANNEL_SIZE

            # Write raw_value u16
            struct.pack_into("<H", mm, offset + 0, raw_value)

            # Write voltage f32
            struct.pack_into("<f", mm, offset + 2, voltage)

            # Set enabled
            struct.pack_into("<B", mm, offset + 6, 1)

            # Increment sequence counter
            seq = struct.unpack_from("<q", mm, SEQ_OFFSET)[0]
            struct.pack_into("<q", mm, SEQ_OFFSET, seq + 1)

        os.close(fd)
        return {
            "channel": channel,
            "raw_value": raw_value,
            "voltage": round(voltage, 4),
            "injected": True,
        }
    except FileNotFoundError:
        return {"error": f"SHM file not found: {SHM_PATH}"}
    except Exception as e:
        return {"error": str(e)}

"""Firmware management endpoints."""

import os
import shutil
from pathlib import Path

from fastapi import APIRouter, Request, UploadFile, File, Form

router = APIRouter()

FIRMWARE_DIR = Path("/opt/volta/firmware")


@router.post("/flash")
async def flash(
    request: Request,
    binary: UploadFile = File(None),
    elf_path: str = Form(None),
    reset: bool = Form(True),
    start: bool = Form(True),
):
    if binary:
        # Upload mode: save file then flash
        dest = FIRMWARE_DIR / "uploads" / binary.filename
        dest.parent.mkdir(parents=True, exist_ok=True)
        with open(dest, "wb") as f:
            shutil.copyfileobj(binary.file, f)
        flash_path = f"/opt/volta/firmware/uploads/{binary.filename}"
    elif elf_path:
        # Path mode: use existing file in container
        flash_path = elf_path
    else:
        return {"success": False, "message": "Provide binary file or elf_path"}

    result = await request.app.state.voltad.flash_firmware(flash_path)

    if result["success"] and start:
        await request.app.state.voltad.start_simulation()
        result["simulation_started"] = True

    return result


@router.get("/info")
async def info():
    # List available firmware files
    files = []
    if FIRMWARE_DIR.exists():
        for p in FIRMWARE_DIR.rglob("*.elf"):
            files.append(str(p.relative_to(FIRMWARE_DIR)))
    return {"firmware_dir": str(FIRMWARE_DIR), "available": files}

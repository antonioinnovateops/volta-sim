# Volta-Sim

Hardware-in-the-loop simulation platform: run real firmware on emulated MCUs, see results in 3D via Pixel Streaming.

## What Works Today

After 5 sprints, the system provides:

| Capability | How |
|-----------|-----|
| STM32F4 firmware execution | Renode emulates STM32F407VG (GPIO, UART, SysTick, timers, PWM, ADC) |
| GPIO output visualization | 4 virtual LEDs (PD12-15) rendered in UE5 via shared memory |
| GPIO input from browser | User button (PA0) clickable in Pixel Streaming view |
| PWM output visualization | Motor actor in UE5 rotates proportional to duty cycle |
| ADC sensor injection | Web slider injects voltage (0-3.3V) into ADC channels via SHM |
| UART capture | USART2 output in web console + ZMQ events |
| Firmware control | REST API + gRPC: flash ELF, start/stop, query status |
| GDB debugging | Connect arm-none-eabi-gdb to port 3333 |
| 3D streaming | UE5 Pixel Streaming to any browser on port 8888 |
| Web dashboard | GPIO + PWM gauge + ADC panel + UART console on port 3000 |
| CLI tool | `volta flash`, `volta gpio`, `volta pwm`, `volta adc`, etc. |

## Architecture

```
volta-mcu              volta-ue5              volta-api        volta-web
  Renode + voltad        UE5 + Pixel Stream     FastAPI          React + nginx
    |                      |                      |                |
    +--- /dev/shm/volta_* -+----------------------+                |
    |                      |                      |                |
    Ports:                Ports:                 Port:            Port:
      1234  Renode          8888  Pixel Stream     8080  REST API   3000  Dashboard
      3333  GDB             8889  WebRTC
      5555  ZMQ PUB
     50051  gRPC
```

**Data flow (LED blink):**
1. Firmware calls `GPIOD->ODR |= (1 << 12)`
2. Renode intercepts → `VoltaGPIOBridge` writes `output_state` to SHM
3. UE5 `VoltaLEDActor` reads SHM every frame → updates emissive material
4. Pixel Streaming encodes frame → browser sees LED turn on

**Data flow (button press):**
1. User clicks button in browser (Pixel Streaming)
2. UE5 `VoltaButtonActor` writes `input_state` to SHM
3. `VoltaGPIOInputBridge` polls SHM at 1ms → drives `gpioPortA@0` in Renode
4. Firmware reads `GPIOA->IDR` → sees button pressed

**Data flow (PWM motor):**
1. Firmware configures TIM1 CH1 PWM (PSC, ARR, CCR1, CCER)
2. `VoltaPWMBridge` polls timer registers at 5ms → computes duty/freq → writes SHM
3. voltad detects SHM change → publishes `volta.pwm.update` via ZMQ
4. UE5 `VoltaMotorActor` reads duty cycle → rotates cylinder at proportional RPM

**Data flow (ADC injection):**
1. User moves slider on web dashboard → POST `/api/v1/adc/0/inject`
2. API writes raw_value + voltage to SHM ADC section
3. Firmware calls `ADC1_CR2 |= SWSTART` → reads `ADC1_DR`
4. `VoltaADCBridge` returns raw_value from SHM → firmware processes sensor data

## Prerequisites

- Linux (Ubuntu 22.04+)
- Docker 24+ with Compose v2
- NVIDIA GPU with driver 535+
- [nvidia-container-toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)
- GitHub account linked to [Epic Games](https://www.unrealengine.com/en-US/ue-on-github) (for UE5 source access)
- ARM cross-compiler (`gcc-arm-none-eabi`) for building firmware

## Quick Start

```bash
# 1. Clone and configure
cd volta-sim
cp .env.example .env
# Edit .env → set GITHUB_TOKEN (required for UE5 source clone)

# 2. Build UE5 engine (one-time, 2-4 hours)
make ue5-engine

# 3. Build UE5 project (minutes)
make ue5-project

# 4. Start the full stack
make up

# 5. Open web dashboard
# http://localhost:3000

# 6. Flash + start via dashboard or CLI
volta flash firmware/examples/blinky/blinky.elf
# Or via REST API:
# curl -X POST http://localhost:8080/api/v1/firmware/flash -F elf_path=/opt/volta/firmware/examples/blinky/blinky.elf -F start=true

# 7. See results
# Dashboard shows: 3D Pixel Stream + GPIO panel (PD12 toggling) + UART console
# Direct Pixel Streaming: http://localhost:8888
```

### MCU-Only Mode (no GPU required)

To run just the MCU emulation + API (no UE5 rendering):

```bash
# Start MCU + API containers only
docker-compose up -d volta-mcu volta-api

# Flash and start via CLI
volta flash firmware/examples/blinky/blinky.elf
volta status
volta gpio

# Or via REST API
curl http://localhost:8080/api/v1/gpio
curl http://localhost:8080/api/v1/uart/1/buffer
```

### CLI Tool

```bash
# Install (symlinks to cli/volta)
make cli-install

# Usage
volta flash firmware/examples/blinky/blinky.elf  # Flash + auto-start
volta start                                        # Start simulation
volta stop                                         # Stop simulation
volta status                                       # Query state
volta gpio                                         # Show GPIO pin states
volta uart                                         # Read UART buffer (default: USART2)
volta uart 0                                       # Read UART channel 0
volta pwm                                          # Show PWM channel states
volta pwm 0                                        # Show single PWM channel
volta adc                                          # Show ADC channel states
volta adc 0                                        # Show single ADC channel
volta adc 0 --inject 1.65                          # Inject voltage into ADC channel
```

## Project Structure

```
volta-sim/
├── api/                        Python REST API (FastAPI)
│   ├── main.py                   App entry point, lifespan, CORS
│   ├── routers/
│   │   ├── simulation.py         /simulation/start, /stop, /status, ws/events
│   │   ├── firmware.py           /firmware/flash (upload or path), /info
│   │   ├── gpio.py               /gpio (reads SHM directly)
│   │   ├── uart.py               /uart/{channel}/buffer (reads SHM ring buffer)
│   │   ├── pwm.py                /pwm (reads PWM state from SHM)
│   │   └── adc.py                /adc + /adc/{ch}/inject (reads/writes ADC via SHM)
│   └── services/
│       ├── voltad_client.py      Async gRPC client for voltad
│       ├── zmq_subscriber.py     ZMQ SUB → WebSocket broadcast
│       └── websocket_manager.py  WebSocket connection pool
│
├── bridge/                     Rust bridge daemon (voltad)
│   ├── proto/volta.proto         gRPC service definition
│   └── src/
│       ├── main.rs               CLI entry point
│       ├── config.rs             CLI argument parsing
│       ├── shm/                  Shared memory client
│       │   ├── layout.rs           SHM layout constants (canonical source)
│       │   ├── header.rs           Header read/write
│       │   ├── gpio.rs            GPIO state polling
│       │   ├── uart.rs            UART ring buffer
│       │   ├── pwm.rs             PWM channel state
│       │   └── adc.rs             ADC channel state
│       ├── events/               ZMQ event publisher
│       ├── renode/               Renode telnet + UART reader
│       └── grpc/                 gRPC server (flash/start/stop/status)
│
├── cli/                        CLI tool
│   └── volta                     Python CLI (volta flash/start/stop/status/gpio/uart/pwm/adc)
│
├── containers/
│   ├── api/                    volta-api container
│   │   └── Dockerfile            Python 3.12 + gRPC proto gen + uvicorn
│   ├── mcu/                    Renode + voltad container
│   │   ├── Dockerfile            2-stage: Rust builder → Ubuntu runtime
│   │   └── entrypoint.sh         Starts Renode, waits, starts voltad
│   ├── ue5/                    UE5 + Pixel Streaming container
│   │   ├── Dockerfile            4-stage: source → engine → project → runtime
│   │   ├── entrypoint.sh         Starts Xvfb + signaling server + UE5
│   │   └── pixel_streaming.json  Signaling server config
│   └── web/                    volta-web container
│       ├── Dockerfile            2-stage: Node build → nginx serve
│       └── nginx.conf            SPA routing + API/WS proxy
│
├── engine/                     UE5 project (VoltaSim)
│   ├── VoltaSim.uproject         Project file (PixelStreaming plugin)
│   ├── Config/                   Engine + game config
│   └── Source/VoltaSim/
│       ├── Shm/
│       │   ├── VoltaShmLayout.h    SHM constants (mirrors layout.rs)
│       │   └── VoltaShmClient.*    POSIX mmap reader/writer
│       ├── Actors/
│       │   ├── VoltaLEDActor.*     Sphere mesh + emissive material, reads SHM
│       │   ├── VoltaButtonActor.*  Cylinder mesh, clickable, writes SHM
│       │   └── VoltaMotorActor.*   Cylinder mesh, rotates by PWM duty cycle
│       └── Board/
│           └── VoltaBoardActor.*   Spawns STM32F4 Discovery layout
│
├── peripherals/                Renode .NET peripherals (C#)
│   ├── VoltaGPIOBridge.cs        GPIO output → SHM (Renode writes)
│   ├── VoltaGPIOInputBridge.cs   SHM → GPIO input (Renode reads)
│   ├── VoltaUARTBridge.cs        UART → SHM ring buffer
│   ├── VoltaPWMBridge.cs         Timer register polling → SHM PWM state
│   └── VoltaADCBridge.cs         ADC register model, reads raw_value from SHM
│
├── platforms/                  Renode platform definitions
│   ├── stm32f4_discovery.repl    Board: 4 LED bridges + input bridge + button
│   └── stm32f4_discovery.resc    Startup: load peripherals, UART, GDB
│
│
├── web/                        React + TypeScript web dashboard
│   ├── src/
│   │   ├── App.tsx               Grid layout: stream + sidebar + console
│   │   ├── components/
│   │   │   ├── PixelStreamViewer.tsx   Embeds Pixel Streaming via iframe
│   │   │   ├── GPIOPanel.tsx           Pin states with LED color indicators
│   │   │   ├── PWMGauge.tsx           SVG semicircle gauge for PWM duty cycle
│   │   │   ├── ADCPanel.tsx           Bar graph + voltage display + injection slider
│   │   │   ├── UARTConsole.tsx         Ring buffer display, channel select
│   │   │   └── SimulationControls.tsx  Flash + Start/Stop buttons, status
│   │   └── hooks/
│   │       └── useWebSocket.ts         Auto-reconnecting WS hook
│   ├── package.json
│   └── vite.config.ts
│
├── firmware/examples/          Pre-built firmware examples
│   ├── blinky/                   Toggles PD12 (green LED) every 500ms
│   ├── uart_hello/               Prints to USART2
│   └── pwm_motor/                TIM1 CH1 PWM sweep + ADC1 CH0 read
│
├── docker-compose.yml          Production: volta-mcu + volta-ue5
├── docker-compose.dev.yml      Development overrides (debug logging)
├── Makefile                    Build/run/flash targets
└── .env.example                Environment variables template
```

## Shared Memory Layout

All components (Rust, C#, C++) share the same memory-mapped file at `/dev/shm/volta_peripheral_state` (65,536 bytes):

| Offset | Size | Field |
|--------|------|-------|
| 0x0000 | 4B | Magic: `0x564F4C54` ("VOLT") |
| 0x0004 | 4B | Version: `1` |
| 0x0008 | 8B | Sequence counter (incremented on every state change) |
| 0x0010 | 4B | Flags (bit 0 = initialized) |
| 0x0040 | 256B | GPIO: 16 ports x 16 bytes (`output_state:u16` + `input_state:u16` + reserved) |
| 0x0140 | 8192B | UART: 8 channels x 1024 bytes (write_head + ring buffer) |
| 0x2140 | 256B | PWM: 16 channels x 16 bytes (`duty_cycle:f32` + `frequency:u32` + `enabled:u8` + `polarity:u8` + `period_us:u32`) |
| 0x2240 | 256B | ADC: 16 channels x 16 bytes (`raw_value:u16` + `voltage:f32` + `enabled:u8` + `sample_rate:u32`) |

**STM32F4 Discovery pin mapping:**
- Port D (index 3): PD12=green LED, PD13=orange, PD14=red, PD15=blue
- Port A (index 0): PA0=user button

Canonical layout definition: [`bridge/src/shm/layout.rs`](bridge/src/shm/layout.rs)

## Makefile Targets

| Target | Description |
|--------|------------|
| `make up` | Start all containers |
| `make down` | Stop all containers |
| `make build` | Build volta-mcu container |
| `make logs` | Follow container logs |
| `make flash FILE=<path>` | Flash ELF to Renode |
| `make firmware` | Cross-compile firmware examples (blinky, uart_hello, pwm_motor) |
| `make test` | Run all E2E tests |
| `make test-blinky` | Run blinky E2E test |
| `make test-pwm` | Run PWM/ADC E2E test |
| `make monitor` | Connect to Renode telnet |
| `make shell` | Shell into volta-mcu |
| `make shell-ue5` | Shell into volta-ue5 |
| `make ue5-engine` | Build UE5 engine image (2-4 hours, cached) |
| `make ue5-project` | Build VoltaSim UE5 project |
| `make stream` | Start stack + print Pixel Streaming URL |
| `make check-gpu` | Verify nvidia-smi works |

## gRPC API

Service `volta.VoltaControl` on port 50051:

| RPC | Description |
|-----|------------|
| `FlashFirmware` | Load ELF binary into Renode (`elf_path` field) |
| `StartSimulation` | Begin MCU execution |
| `StopSimulation` | Halt MCU and reset |
| `GetStatus` | Query simulation state (running/paused/stopped) |

Example with [grpcurl](https://github.com/fullstorydev/grpcurl):

```bash
# Flash firmware
grpcurl -plaintext -d '{"elf_path": "/opt/volta/firmware/examples/blinky/blinky.elf"}' \
  localhost:50051 volta.VoltaControl/FlashFirmware

# Start
grpcurl -plaintext -d '{}' localhost:50051 volta.VoltaControl/StartSimulation

# Check status
grpcurl -plaintext -d '{}' localhost:50051 volta.VoltaControl/GetStatus
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GITHUB_TOKEN` | (required) | GitHub PAT with EpicGames/UnrealEngine access |
| `UE5_BRANCH` | `5.4` | UE5 branch to build |
| `RENODE_TELNET_PORT` | `1234` | Renode monitor port |
| `RENODE_GDB_PORT` | `3333` | GDB server port |
| `VOLTAD_ZMQ_PUB_PORT` | `5555` | ZMQ event publish port |
| `VOLTAD_GRPC_PORT` | `50051` | gRPC control port |
| `PIXEL_STREAM_PORT` | `8888` | Pixel Streaming HTTP port |
| `PIXEL_STREAM_SIGNAL_PORT` | `8889` | WebRTC signaling port |
| `LOG_LEVEL` | `info` | Logging verbosity |

## Development History

### Sprint 1-2: MCU Emulation + Bridge
- Renode emulates STM32F4 Discovery (GPIO, UART, SysTick)
- VoltaGPIOBridge writes GPIO state to shared memory
- VoltaUARTBridge captures USART2 output to SHM ring buffer
- voltad (Rust): polls SHM changes, publishes ZMQ events
- voltad: UART TCP reader, gRPC control server (flash/start/stop/status)
- Docker container with healthcheck, GDB server

### Sprint 3: UE5 + LED Actor + Pixel Streaming
- UE5 project with PixelStreaming plugin
- POSIX SHM client (C++) with volatile reads for cross-frame consistency
- VoltaLEDActor: reads GPIO output state, updates emissive material
- VoltaButtonActor: writes GPIO input state on click (momentary/toggle)
- VoltaBoardActor: spawns STM32F4 Discovery layout (4 LEDs + 1 button)
- VoltaGPIOInputBridge: polls SHM input_state, drives Renode GPIO pins
- Multi-stage Dockerfile (engine build cached separately from project)
- Xvfb + signaling server + UE5 entrypoint

### Sprint 4: REST API + Web Dashboard + CLI
- volta-api container (Python FastAPI) wrapping voltad gRPC
- REST endpoints: /simulation/start|stop|status, /firmware/flash, /gpio, /uart
- WebSocket real-time events (ZMQ SUB → WebSocket broadcast)
- SHM-direct reads for GPIO and UART state (no gRPC round-trip)
- volta-web container (React + TypeScript + Vite → nginx)
- Web dashboard: Pixel Streaming embed + GPIO panel + UART console + simulation controls
- CLI tool (`volta flash/start/stop/status/gpio/uart`) — zero dependencies
- Phase 1 "Blinky" complete: all 4 containers operational

### Sprint 5: PWM + ADC + Motor Actor (Phase 2 Foundation)
- VoltaPWMBridge: polls TIM registers via SystemBus, computes duty/freq, writes SHM
- VoltaADCBridge: BasicDoubleWordPeripheral at ADC1 base, returns SHM raw_value on DR read
- SHM layout extended: PWM (0x2140, 16 channels) + ADC (0x2240, 16 channels)
- Rust bridge: pwm.rs + adc.rs modules, ZMQ events (volta.pwm.update, volta.adc.change)
- VoltaMotorActor: cylinder mesh rotates at RPM = duty_cycle * MaxRPM
- REST API: /pwm, /pwm/{ch}, /adc, /adc/{ch}, /adc/{ch}/inject
- Web dashboard: PWM semicircle gauge + ADC bar graph with injection slider
- CLI: `volta pwm`, `volta adc`, `volta adc --inject <voltage>`
- PWM motor firmware example (TIM1 CH1 sweep + ADC1 CH0 read)
- E2E tests: test_pwm_e2e.sh, test_blinky_e2e.sh
- Phase 2 "Motion" foundation: firmware can drive motor + read sensors through SHM

## Disk Space

| Component | Size |
|-----------|------|
| UE5 engine build | ~80-100 GB |
| volta-ue5 runtime image | ~15-20 GB |
| volta-mcu image | ~2 GB |
| Shader cache (first run) | ~5 GB |

Ensure at least **150 GB** free disk space for the initial build.

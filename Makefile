.PHONY: build up down flash logs clean firmware test test-pwm test-blinky
.PHONY: api web cli-install ue5-package ue5-engine ue5-build ue5-up ue5-down ue5-logs

COMPOSE := WEB_PORT=3001 docker-compose
COMPOSE_UE5 := $(COMPOSE) -f docker-compose.yml -f docker-compose.ue5.yml
FILE ?=

# --- Container targets ---

build:
	$(COMPOSE) build volta-mcu volta-api volta-web

up:
	$(COMPOSE) up -d

down:
	$(COMPOSE) down

logs:
	$(COMPOSE) logs -f

# --- API + Web targets ---

api:
	$(COMPOSE) build volta-api
	$(COMPOSE) up -d volta-api

web:
	$(COMPOSE) build volta-web
	$(COMPOSE) up -d volta-web

cli-install:
	@echo "Installing volta CLI..."
	ln -sf $(CURDIR)/cli/volta /usr/local/bin/volta 2>/dev/null || \
		ln -sf $(CURDIR)/cli/volta ~/.local/bin/volta
	@echo "Done. Run: volta status"

# --- UE5 Pixel Streaming (optional, requires NVIDIA GPU + packaged game) ---

UE_IMAGE ?= ghcr.io/epicgames/unreal-engine:dev-5.4

ue5-package:
	@echo "Packaging VoltaSim for Linux..."
	@echo "  UE_IMAGE=$(UE_IMAGE)"
	@echo "  Override: make ue5-package UE_IMAGE=adamrehn/ue4-minimal:5.4.4"
	docker build -f containers/ue5/Dockerfile.package \
		--build-arg UE_IMAGE=$(UE_IMAGE) \
		-t volta-packager .
	@mkdir -p packaged-game
	docker run --rm -v $$(pwd)/packaged-game:/output volta-packager \
		sh -c "cp -r /game/* /output/ 2>/dev/null || true"
	@echo "Packaged game in packaged-game/"

ue5-engine:
	@echo "Building UE5 engine from source (6-12 hours)..."
	@echo "  This creates adamrehn/ue4-minimal:5.4.4 Docker image"
	ue4-docker build 5.4.4 --target minimal \
		--exclude ddc --exclude debug --exclude templates \
		-username $$(git config user.name 2>/dev/null || echo "user") \
		-password $$(gh auth token)

ue5-build:
	@echo "Building UE5 containers (signaling + runtime)..."
	@test -d packaged-game && test "$$(ls -A packaged-game/ 2>/dev/null | grep -v .gitkeep)" != "" \
		|| (echo "ERROR: No packaged game in packaged-game/." && \
		    echo "  Run: make ue5-package  (requires Epic Games Docker image)" && \
		    echo "  Or manually: package in UE5 Editor → copy to packaged-game/" && exit 1)
	$(COMPOSE_UE5) build volta-ue5-signaling volta-ue5

ue5-up: up
	@echo "Starting UE5 Pixel Streaming..."
	$(COMPOSE_UE5) up -d volta-ue5-signaling volta-ue5
	@echo "Pixel Streaming: http://localhost:$${PIXEL_STREAM_PORT:-8888}"
	@echo "Web dashboard:   http://localhost:3001"

ue5-down:
	$(COMPOSE_UE5) down

ue5-logs:
	$(COMPOSE_UE5) logs -f volta-ue5 volta-ue5-signaling

# --- Firmware targets ---

firmware:
	docker exec volta-mcu make -C /opt/volta/firmware/examples/blinky
	docker exec volta-mcu make -C /opt/volta/firmware/examples/uart_hello
	docker exec volta-mcu bash -c "cp -r /opt/volta/firmware/examples/pwm_motor /tmp/pwm_motor 2>/dev/null; make -C /tmp/pwm_motor"

firmware-clean:
	docker exec volta-mcu make -C /opt/volta/firmware/examples/blinky clean || true
	docker exec volta-mcu make -C /opt/volta/firmware/examples/uart_hello clean || true

flash:
ifndef FILE
	$(error FILE is required. Usage: make flash FILE=firmware/examples/blinky/blinky.elf)
endif
	@echo "Flashing $(FILE)..."
	curl -sf -X POST http://localhost:8080/api/v1/firmware/flash \
		-F "elf_path=/opt/volta/$(FILE)" | python3 -m json.tool

# --- Development ---

shell:
	docker exec -it volta-mcu bash

monitor:
	docker exec -it volta-mcu socat - TCP:localhost:1234

# --- Testing ---

test: test-blinky test-pwm

test-blinky:
	@echo "Running blinky E2E test..."
	bash tests/test_blinky_e2e.sh

test-pwm:
	@echo "Running PWM/ADC E2E test..."
	bash tests/test_pwm_e2e.sh

clean: down firmware-clean
	docker system prune -f

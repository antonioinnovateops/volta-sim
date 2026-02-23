.PHONY: build up down flash logs clean firmware test test-pwm test-blinky
.PHONY: ue5-engine ue5-project stream check-gpu
.PHONY: api web cli-install

COMPOSE := docker-compose
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

# --- UE5 targets ---

ue5-engine: check-gpu
	@echo "Building UE5 engine (this takes 2-4 hours on first run)..."
	docker build \
		--target ue5-engine \
		--build-arg GITHUB_TOKEN=$${GITHUB_TOKEN} \
		--build-arg UE5_BRANCH=$${UE5_BRANCH:-5.4} \
		-t volta-ue5-engine:$${UE5_BRANCH:-5.4} \
		-f containers/ue5/Dockerfile .

ue5-project: check-gpu
	@echo "Building VoltaSim UE5 project..."
	$(COMPOSE) build volta-ue5

stream: up
	@echo "Pixel Streaming available at http://localhost:$${PIXEL_STREAM_PORT:-8888}"
	@echo "Web dashboard available at http://localhost:$${WEB_PORT:-3000}"

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

check-gpu:
	@echo "Checking GPU availability..."
	@nvidia-smi > /dev/null 2>&1 || \
		(echo "WARNING: nvidia-smi not found. GPU passthrough may not work." && \
		 echo "For CI, use NullRHI: add -nullrhi to UE5 launch args." && \
		 echo "")

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

shell-ue5:
	docker exec -it volta-ue5 bash

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

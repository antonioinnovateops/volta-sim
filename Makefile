.PHONY: build up down flash logs clean firmware test test-pwm test-blinky
.PHONY: api web cli-install

COMPOSE := WEB_PORT=3001 docker-compose
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

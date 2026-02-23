.PHONY: build up down flash logs clean firmware test

COMPOSE := docker-compose
FILE ?=

# --- Container targets ---

build:
	$(COMPOSE) build

up:
	$(COMPOSE) up -d

down:
	$(COMPOSE) down

logs:
	$(COMPOSE) logs -f

# --- Firmware targets ---

firmware:
	$(MAKE) -C firmware/examples/blinky
	$(MAKE) -C firmware/examples/uart_hello

firmware-clean:
	$(MAKE) -C firmware/examples/blinky clean
	$(MAKE) -C firmware/examples/uart_hello clean

flash:
ifndef FILE
	$(error FILE is required. Usage: make flash FILE=firmware/examples/blinky/blinky.elf)
endif
	@echo "Flashing $(FILE) to Renode..."
	docker exec volta-mcu sh -c '\
		echo "machine LoadELF @/opt/volta/$(FILE)" | \
		socat - TCP:localhost:1234'

# --- Development ---

shell:
	docker exec -it volta-mcu bash

monitor:
	docker exec -it volta-mcu socat - TCP:localhost:1234

# --- Testing ---

test:
	@echo "Running integration tests..."
	bash tests/test_blinky_e2e.sh

clean: down firmware-clean
	docker system prune -f

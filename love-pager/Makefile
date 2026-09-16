WEST_BIN ?= ../.venv/bin/west

BOARD ?= esp32c3_supermini/esp32c3
BUILD_DIR ?= build

build:
	$(WEST_BIN) build \
		-b $(BOARD) \
		-d $(BUILD_DIR) \
		.

flash:
	$(WEST_BIN) flash -d $(BUILD_DIR)

rebuild:
	$(WEST_BIN) build \
		-p always \
		-b $(BOARD) \
		-d $(BUILD_DIR) \
		.

monitor:
	$(WEST_BIN) espressif monitor

menuconfig:
	$(WEST_BIN) build -d $(BUILD_DIR) -t menuconfig

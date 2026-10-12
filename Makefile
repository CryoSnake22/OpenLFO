.PHONY: build run clean
PROJECT_NAME = OpenLFO
PRODUCT_NAME = OpenLFO
BUILD_DIR = build
CONFIG ?= Debug

build:
	@cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(CONFIG)
	@cmake --build $(BUILD_DIR)

run: build
	@open "./$(BUILD_DIR)/$(PROJECT_NAME)_artefacts/$(CONFIG)/Standalone/$(PRODUCT_NAME).app"

clean:
	@rm -rf $(BUILD_DIR)

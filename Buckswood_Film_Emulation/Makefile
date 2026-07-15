PLUGIN_NAME := BuckswoodFilmEmulation
BUNDLE_NAME := $(PLUGIN_NAME).ofx.bundle
OFX_BINARY := $(PLUGIN_NAME).ofx

ROOT_DIR := ..
OFX_INCLUDE := $(ROOT_DIR)/third_party/openfx/include
BUILD_DIR := build
DIST_DIR := dist
BUNDLE_DIR := $(DIST_DIR)/$(BUNDLE_NAME)

CXX ?= clang++
ARCHFLAGS ?= -arch arm64 -arch x86_64
CXXFLAGS := -std=c++17 -O3 -fvisibility=hidden -Wall -Wextra \
	$(ARCHFLAGS) -Iinclude -I$(OFX_INCLUDE)
LDFLAGS_BUNDLE := -bundle

CORE_SRC := src/FilmEmulationCore.cpp
OFX_SRC := src/BuckswoodFilmEmulationOFX.cpp
SMOKE_SRC := src/film_emulation_smoketest.cpp

.PHONY: all bundle smoketest descriptor-test install install-user clean check-toolchain

all: bundle smoketest descriptor-test

check-toolchain:
	@$(CXX) --version >/dev/null

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(DIST_DIR):
	mkdir -p $(DIST_DIR)

$(BUILD_DIR)/$(OFX_BINARY): $(CORE_SRC) $(OFX_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS_BUNDLE) $(CORE_SRC) $(OFX_SRC) -o $@

bundle: $(BUILD_DIR)/$(OFX_BINARY) | $(DIST_DIR)
	rm -rf $(BUNDLE_DIR)
	mkdir -p $(BUNDLE_DIR)/Contents/MacOS
	cp Info.plist $(BUNDLE_DIR)/Contents/Info.plist
	cp $(BUILD_DIR)/$(OFX_BINARY) $(BUNDLE_DIR)/Contents/MacOS/$(OFX_BINARY)
	@echo "Built $(BUNDLE_DIR)"

$(BUILD_DIR)/film_emulation_smoketest: $(CORE_SRC) $(SMOKE_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SRC) $(SMOKE_SRC) -o $@

smoketest: $(BUILD_DIR)/film_emulation_smoketest
	$(BUILD_DIR)/film_emulation_smoketest

$(BUILD_DIR)/ofx_descriptor_smoketest: src/ofx_descriptor_smoketest.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 -O2 -I$(OFX_INCLUDE) $< -o $@

descriptor-test: bundle $(BUILD_DIR)/ofx_descriptor_smoketest
	$(BUILD_DIR)/ofx_descriptor_smoketest \
		$(BUNDLE_DIR)/Contents/MacOS/$(OFX_BINARY)

install: bundle
	mkdir -p /Library/OFX/Plugins
	rm -rf /Library/OFX/Plugins/$(BUNDLE_NAME)
	cp -R $(BUNDLE_DIR) /Library/OFX/Plugins/$(BUNDLE_NAME)
	@echo "Installed /Library/OFX/Plugins/$(BUNDLE_NAME)"

install-user: bundle
	mkdir -p "$(HOME)/Library/OFX/Plugins"
	rm -rf "$(HOME)/Library/OFX/Plugins/$(BUNDLE_NAME)"
	cp -R $(BUNDLE_DIR) "$(HOME)/Library/OFX/Plugins/$(BUNDLE_NAME)"
	@echo "Installed $(HOME)/Library/OFX/Plugins/$(BUNDLE_NAME)"

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)

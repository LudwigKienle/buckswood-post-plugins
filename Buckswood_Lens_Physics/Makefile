PLUGIN_NAME := BuckswoodLensPhysics
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

CORE_SRC := src/LensPhysicsCore.cpp
OFX_SRC := src/BuckswoodLensPhysicsOFX.cpp
SMOKE_SRC := src/lens_physics_smoketest.cpp

.PHONY: all bundle smoketest install install-user dctl-install clean check-toolchain

all: bundle smoketest

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

$(BUILD_DIR)/lens_physics_smoketest: $(CORE_SRC) $(SMOKE_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SRC) $(SMOKE_SRC) -o $@

smoketest: $(BUILD_DIR)/lens_physics_smoketest
	$(BUILD_DIR)/lens_physics_smoketest

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

dctl-install:
	cp dctl/Buckswood_Lens_Physics_v01.dctl "/Library/Application Support/Blackmagic Design/DaVinci Resolve/LUT/Buckswood_Lens_Physics_v01.dctl"
	@echo "Installed DCTL to Resolve LUT folder"

clean:
	rm -rf $(BUILD_DIR) $(DIST_DIR)

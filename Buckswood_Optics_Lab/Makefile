PLUGIN_NAME := BuckswoodOpticsLab
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
	$(ARCHFLAGS) -Iinclude -Ithird_party -I$(ROOT_DIR)/shared -I$(OFX_INCLUDE)
LDFLAGS_BUNDLE := -bundle -framework Metal -framework Foundation

CORE_SRC := src/OpticsLabCore.cpp
ASSET_SRC := src/OpticsAssetLibrary.cpp
OFX_SRC := src/BuckswoodOpticsLabOFX.cpp
METAL_SRC := src/OpticsLabMetal.mm
SMOKE_SRC := src/optics_lab_smoketest.cpp
METAL_TEST_SRC := src/optics_lab_metal_test.mm

.PHONY: all bundle smoketest metaltest descriptor-test asset-test benchmark licensed-asset-test install install-user clean

all: bundle smoketest metaltest descriptor-test asset-test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(DIST_DIR):
	mkdir -p $(DIST_DIR)

$(BUILD_DIR)/$(OFX_BINARY): $(CORE_SRC) $(ASSET_SRC) $(OFX_SRC) $(METAL_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS_BUNDLE) $(CORE_SRC) $(ASSET_SRC) $(OFX_SRC) $(METAL_SRC) -o $@

bundle: $(BUILD_DIR)/$(OFX_BINARY) | $(DIST_DIR)
	rm -rf $(BUNDLE_DIR)
	mkdir -p $(BUNDLE_DIR)/Contents/MacOS
	cp Info.plist $(BUNDLE_DIR)/Contents/Info.plist
	cp $(BUILD_DIR)/$(OFX_BINARY) $(BUNDLE_DIR)/Contents/MacOS/$(OFX_BINARY)
	@echo "Built $(BUNDLE_DIR)"

$(BUILD_DIR)/optics_lab_smoketest: $(CORE_SRC) $(SMOKE_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SRC) $(SMOKE_SRC) -o $@

smoketest: $(BUILD_DIR)/optics_lab_smoketest
	$(BUILD_DIR)/optics_lab_smoketest

$(BUILD_DIR)/optics_lab_metal_test: $(CORE_SRC) $(ASSET_SRC) $(METAL_SRC) $(METAL_TEST_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -framework Metal -framework Foundation \
		$(CORE_SRC) $(ASSET_SRC) $(METAL_SRC) $(METAL_TEST_SRC) -o $@

metaltest: $(BUILD_DIR)/optics_lab_metal_test
	$(BUILD_DIR)/optics_lab_metal_test

$(BUILD_DIR)/ofx_descriptor_smoketest: src/ofx_descriptor_smoketest.cpp | $(BUILD_DIR)
	$(CXX) -std=c++17 -O2 -I$(OFX_INCLUDE) $< -o $@

descriptor-test: bundle $(BUILD_DIR)/ofx_descriptor_smoketest
	$(BUILD_DIR)/ofx_descriptor_smoketest \
		$(BUNDLE_DIR)/Contents/MacOS/$(OFX_BINARY)

$(BUILD_DIR)/asset_library_smoketest: $(ASSET_SRC) src/asset_library_smoketest.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(ASSET_SRC) src/asset_library_smoketest.cpp -o $@

asset-test: $(BUILD_DIR)/asset_library_smoketest
	$(BUILD_DIR)/asset_library_smoketest

$(BUILD_DIR)/optics_lab_benchmark: $(CORE_SRC) src/optics_lab_benchmark.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SRC) src/optics_lab_benchmark.cpp -o $@

benchmark: $(BUILD_DIR)/optics_lab_benchmark
	$(BUILD_DIR)/optics_lab_benchmark

licensed-asset-test: asset-test
	@test -n "$(GLASS_ASSET_ROOT)" || \
		(echo "Set GLASS_ASSET_ROOT=/path/to/glass" && exit 1)
	$(BUILD_DIR)/asset_library_smoketest "$(GLASS_ASSET_ROOT)"

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

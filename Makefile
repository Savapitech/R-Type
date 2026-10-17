include utils.mk

BUILD_DIR	= build
BUILD_TYPE	?= Release
BINS		= bin r-type_client r-type_server

# embedded builds live in their own dirs: they use static triplets and must
# not share a cmake cache with the dev builds
EMBED_DIR	= build-embedded
ifeq ($(OS),Windows_NT)
EMBED_TRIPLET	?= x64-windows-static
endif

VCPKG_DIR	= $(subst \,/,$(VCPKG_ROOT))

DOCS_THEME_DIR	= External/doxygen-awesome-css
DOCS_THEME_TAG	= v2.3.4

all: build

build: check_vcpkg
	@ cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake
	@ cmake --build $(BUILD_DIR) --config $(BUILD_TYPE)
	@ $(LOG_TIME) "$(C_BLUE) OK $(C_GREEN) build finished $(C_RESET)"

# $(call cmake_build,<build dir>,<target>,<extra cmake args>)
define cmake_build
	@ cmake -B $(1) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake $(3)
	@ cmake --build $(1) --config $(BUILD_TYPE) --target $(2)
	@ $(LOG_TIME) "$(C_BLUE) OK $(C_GREEN) $(2) built $(C_RESET)"
endef

# the server does not need Vulkan nor the graphical lib
server: check_vcpkg
	$(call cmake_build,$(BUILD_DIR)-server,r-type_server,-DRTYPE_BUILD_CLIENT=OFF)

client: check_vcpkg
	$(call cmake_build,$(BUILD_DIR),r-type_client,-DRTYPE_BUILD_CLIENT=ON)

# single binary: libs and assets are linked in, no assets/ folder needed
server_embedded: check_vcpkg
	$(call cmake_build,$(EMBED_DIR)-server,r-type_server_embedded,-DRTYPE_BUILD_CLIENT=OFF $(if $(EMBED_TRIPLET),-DVCPKG_TARGET_TRIPLET=$(EMBED_TRIPLET)))

client_embedded: check_vcpkg
	$(call cmake_build,$(EMBED_DIR)-client,r-type_client_embedded,-DRTYPE_BUILD_CLIENT=ON -DRTYPE_EMBEDDED=ON $(if $(EMBED_TRIPLET),-DVCPKG_TARGET_TRIPLET=$(EMBED_TRIPLET)))

check_vcpkg:
ifndef VCPKG_ROOT
	$(error VCPKG_ROOT is not set. Install vcpkg and set VCPKG_ROOT=/path/to/vcpkg)
endif

debug:
	@ $(MAKE) BUILD_TYPE=Debug build

clean:
	@ cmake -E rm -rf $(BUILD_DIR) CMakeCache.txt CMakeFiles cmake_install.cmake _deps
	@ $(LOG_TIME) "$(C_YELLOW) RM $(C_PURPLE) cmake generated files $(C_RESET)"

fclean: clean
	@ cmake -E rm -rf External $(BUILD_DIR) $(BUILD_DIR)-server $(EMBED_DIR)-server $(EMBED_DIR)-client $(BINS)
	@ $(LOG_TIME) "$(C_YELLOW) RM $(C_PURPLE) External and binaries $(C_RESET)"

.NOTPARALLEL:
re: fclean all

format:
	@ clang-format -i $(shell find src lib -name "*.cpp" -o -name "*.hpp")
	@ $(LOG_TIME) "$(C_BLUE) CF $(C_GREEN) code formatted $(C_RESET)"

$(DOCS_THEME_DIR):
	@ git -c advice.detachedHead=false clone --quiet --depth 1 --branch $(DOCS_THEME_TAG) \
		https://github.com/jothepro/doxygen-awesome-css.git $(DOCS_THEME_DIR)

docs: $(DOCS_THEME_DIR)
	@ cmake -E make_directory build/docs
	@ doxygen docs/Doxyfile
	@ $(LOG_TIME) "$(C_BLUE) OK $(C_GREEN) docs written to build/docs/html $(C_RESET)"

.PHONY: all build server client server_embedded client_embedded check_vcpkg debug clean fclean re format docs

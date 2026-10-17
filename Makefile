include utils.mk

BUILD_DIR	= build
BUILD_TYPE	?= Release
BINS		= bin r-type_client r-type_server

VCPKG_DIR	= $(subst \,/,$(VCPKG_ROOT))

DOCS_THEME_DIR	= External/doxygen-awesome-css
DOCS_THEME_TAG	= v2.3.4

all: build

build: check_vcpkg
	@ cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_DIR)/scripts/buildsystems/vcpkg.cmake
	@ cmake --build $(BUILD_DIR) --config $(BUILD_TYPE)
	@ $(LOG_TIME) "$(C_BLUE) OK $(C_GREEN) build finished $(C_RESET)"

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
	@ cmake -E rm -rf External build $(BINS)
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

.PHONY: all build check_vcpkg debug clean fclean re format docs

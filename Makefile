include utils.mk

BUILD_DIR	= build
BUILD_TYPE	?= Release
BINS		= bin r-type_client r-type_server

all: build

build: check_vcpkg
	@ cmake -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake
	@ $(MAKE) --no-print-directory -C $(BUILD_DIR)
	@ $(LOG_TIME) "$(C_BLUE) OK $(C_GREEN) build finished $(C_RESET)"

check_vcpkg:
	@ test -n "$(VCPKG_ROOT)" || (echo "$(C_RED)Error:$(C_RESET) VCPKG_ROOT is not set" && exit 1)

debug:
	@ $(MAKE) BUILD_TYPE=Debug build

clean:
	@ rm -rf $(BUILD_DIR) CMakeCache.txt CMakeFiles cmake_install.cmake _deps
	@ $(LOG_TIME) "$(C_YELLOW) RM $(C_PURPLE) cmake generated files $(C_RESET)"

fclean: clean
	@ rm -rf External $(BINS)
	@ $(LOG_TIME) "$(C_YELLOW) RM $(C_PURPLE) External and binaries $(C_RESET)"

re: fclean all

format:
	@ find src lib -name "*.cpp" -o -name "*.hpp" 2>/dev/null | xargs -r clang-format -i
	@ $(LOG_TIME) "$(C_BLUE) CF $(C_GREEN) code formatted $(C_RESET)"

.PHONY: all build check_vcpkg debug clean fclean re format

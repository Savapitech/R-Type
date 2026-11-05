ROOT_DIR := $(abspath ..)

CXX ?= g++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra \
	-Wno-missing-field-initializers -Wno-reorder \
	-I $(ROOT_DIR)/pong/common \
	-I $(ROOT_DIR)/pong/client \
	-I $(ROOT_DIR)/pong/server \
	-I $(ROOT_DIR)/lib/GameEngine/include \
	-I $(ROOT_DIR)/External/ECSLib/include \
	-isystem $(ROOT_DIR)/External/GraphicLib/include \
	-isystem $(ROOT_DIR)/vcpkg_installed/x64-linux/include \
	-isystem $(ROOT_DIR)/build/vcpkg_installed/x64-linux/include

CLIENT_LDFLAGS := -Wl,-rpath,$(ROOT_DIR)/External/GraphicLib/lib:$(ROOT_DIR)/vcpkg_installed/x64-linux/lib:$(ROOT_DIR)/build/vcpkg_installed/x64-linux/lib \
	$(ROOT_DIR)/External/GraphicLib/lib/librtk_graphical.so \
	-L$(ROOT_DIR)/vcpkg_installed/x64-linux/lib \
	-L$(ROOT_DIR)/build/vcpkg_installed/x64-linux/lib \
	-lvulkan -lpthread

SERVER_LDFLAGS := -lpthread

CLIENT_SRC := client/main.cpp \
	client/game.cpp \
	client/scene/menuScene.cpp \
	client/scene/gameScene.cpp

SERVER_SRC := server/main.cpp \
	server/game.cpp

CLIENT_OBJ := $(CLIENT_SRC:.cpp=.o)
SERVER_OBJ := $(SERVER_SRC:.cpp=.o)

CLIENT_TARGET := $(ROOT_DIR)/bin/pong
SERVER_TARGET := $(ROOT_DIR)/bin/pong_server

all: $(CLIENT_TARGET) $(SERVER_TARGET)

$(CLIENT_TARGET): $(CLIENT_OBJ)
	@mkdir -p $(ROOT_DIR)/bin
	@echo "==> Linking $(CLIENT_TARGET)..."
	$(CXX) $(CLIENT_OBJ) -o $(CLIENT_TARGET) $(CLIENT_LDFLAGS)
	@echo "==> Build complete: $(CLIENT_TARGET)"

$(SERVER_TARGET): $(SERVER_OBJ)
	@mkdir -p $(ROOT_DIR)/bin
	@echo "==> Linking $(SERVER_TARGET)..."
	$(CXX) $(SERVER_OBJ) -o $(SERVER_TARGET) $(SERVER_LDFLAGS)
	@echo "==> Build complete: $(SERVER_TARGET)"

%.o: %.cpp
	@echo "==> Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "==> Cleaning object files"
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ)

fclean: clean
	@echo "==> Removing binaries"
	rm -f $(CLIENT_TARGET) $(SERVER_TARGET)

re: fclean all

.PHONY: all clean fclean re

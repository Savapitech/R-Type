cmake_minimum_required(VERSION 3.20)
project(Pong LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable verbose compilation so compiler commands are always displayed
set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(Threads REQUIRED)

file(GLOB_RECURSE PONG_CLIENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/client/*.cpp"
)

file(GLOB_RECURSE PONG_SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp"
)

# Pong Client
add_executable(pong ${PONG_CLIENT_SOURCES})

set_target_properties(pong PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
)

target_include_directories(pong PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/client"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/GameEngine/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/ECSLib/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-linux/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/vcpkg_installed/x64-linux/include"
)

target_link_directories(pong PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-linux/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/vcpkg_installed/x64-linux/lib"
)

target_link_libraries(pong PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib/librtk_graphical.so"
    vulkan
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

set_target_properties(pong PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-linux/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib:${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-linux/lib"
)

# Audio Plugins
add_library(pong_audio_rtk SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/audio_rtk/AudioRtk.cpp"
)
set_target_properties(pong_audio_rtk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin/plugins"
)
target_include_directories(pong_audio_rtk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/GameEngine/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/include"
)
target_link_directories(pong_audio_rtk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib"
)
target_link_libraries(pong_audio_rtk PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib/librtk_graphical.so"
    Threads::Threads
)
set_target_properties(pong_audio_rtk PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib"
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../External/GraphicLib/lib"
)

add_library(pong_audio_dummy SHARED
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/audio_dummy/AudioDummy.cpp"
)
set_target_properties(pong_audio_dummy PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin/plugins"
)
target_include_directories(pong_audio_dummy PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/GameEngine/include"
)

# Pong Server
add_executable(pong_server ${PONG_SERVER_SOURCES})

set_target_properties(pong_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
)

target_include_directories(pong_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/server"
    "${CMAKE_CURRENT_SOURCE_DIR}/../lib/GameEngine/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../External/ECSLib/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../vcpkg_installed/x64-linux/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../build/vcpkg_installed/x64-linux/include"
)

target_link_libraries(pong_server PRIVATE
    Threads::Threads
)

add_custom_target(fclean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_CURRENT_SOURCE_DIR}/../bin/pong" "${CMAKE_CURRENT_SOURCE_DIR}/../bin/pong_server"
)

add_custom_target(re
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
)

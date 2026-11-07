cmake_minimum_required(VERSION 3.20)

option(RTYPE_BUILD_CLIENT "Build the client (needs Vulkan and GraphicLib)" ON)

if(RTYPE_BUILD_CLIENT)
    list(APPEND VCPKG_MANIFEST_FEATURES "client")
endif()

option(RTYPE_BUILD_BENCH "Build tools/compression_bench (needs zstd, lz4, brotli, lzma, zlib)" OFF)
if(RTYPE_BUILD_BENCH)
    list(APPEND VCPKG_MANIFEST_FEATURES "bench")
endif()

list(APPEND VCPKG_INSTALL_OPTIONS "--no-print-usage")
list(REMOVE_DUPLICATES VCPKG_INSTALL_OPTIONS)

set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed" CACHE PATH "")

project(R-Type VERSION 0.1 LANGUAGES CXX)
#force CMP0135 new force a timestamps on fetch
cmake_policy(SET CMP0135 NEW)

#use c++ 20
set(CMAKE_CXX_STANDARD 20)
#force c++ 20
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RTYPE_SANITIZE_THREAD "Build with ThreadSanitizer to detect data races" OFF)
if(RTYPE_SANITIZE_THREAD)
    add_compile_options(-fsanitize=thread -g -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

#Output directory for executable
set(RTYPE_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)
if(RTYPE_SANITIZE_THREAD)
    set(RTYPE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${RTYPE_OUTPUT_DIR})
foreach(CONFIG_NAME DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_NAME} ${RTYPE_OUTPUT_DIR})
endforeach()

if(MSVC AND VCPKG_TARGET_TRIPLET MATCHES "static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

option(RTYPE_EMBEDDED "Also fetch what the embedded client needs (static GraphicLib)" OFF)

if(RTYPE_BUILD_CLIENT)
    find_package(Vulkan REQUIRED)
endif()

#load fetchContent module
include(FetchContent)

if(WIN32)
  add_compile_definitions(
    _WIN32_WINNT=0x0A00
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
endif()

if(RTYPE_BUILD_CLIENT)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|ARM64|aarch64)$")
        set(GRAPHIC_ARCH "arm64")
    else()
        set(GRAPHIC_ARCH "x86_64")
    endif()

    set(GRAPHIC_URL_BASE "https://github.com/rtk-lib/graphical/releases/latest/download" CACHE STRING "Where the prebuilt graphical lib are downloaded from")
    if(WIN32)
        set(GRAPHIC_PLATFORM "windows-${GRAPHIC_ARCH}")
        set(GRAPHIC_ARCHIVE_EXT "zip")
        set(GRAPHIC_BIN_NAME "bin/rtk_graphical.dll")
        set(GRAPHIC_IMPLIB_NAME "lib/rtk_graphical.lib")
        set(GRAPHIC_STATIC_NAME "lib/rtk_graphical_static.lib")
    elseif(APPLE)
        set(GRAPHIC_PLATFORM "macos-${GRAPHIC_ARCH}")
        set(GRAPHIC_ARCHIVE_EXT "tar.gz")
        set(GRAPHIC_BIN_NAME "lib/librtk_graphical.dylib")
        set(GRAPHIC_STATIC_NAME "lib/librtk_graphical.a")
    elseif(UNIX AND NOT APPLE)
        set(GRAPHIC_PLATFORM "linux-${GRAPHIC_ARCH}")
        set(GRAPHIC_ARCHIVE_EXT "tar.gz")
        set(GRAPHIC_BIN_NAME "lib/librtk_graphical.so")
        set(GRAPHIC_STATIC_NAME "lib/librtk_graphical.a")
    endif()
    set(GRAPHIC_URL "${GRAPHIC_URL_BASE}/rtk_graphical-${GRAPHIC_PLATFORM}.${GRAPHIC_ARCHIVE_EXT}")
    set(GRAPHIC_STATIC_URL "${GRAPHIC_URL_BASE}/rtk_graphical-${GRAPHIC_PLATFORM}-static.${GRAPHIC_ARCHIVE_EXT}")

    FetchContent_Declare(
        GraphicLib
        URL ${GRAPHIC_URL}
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/GraphicLib
    )

    #Get variable defined by the FetchContent_Declare
    FetchContent_GetProperties(GraphicLib)

    #check if the lib is already dowload
    if(NOT graphiclib_POPULATED)
        FetchContent_MakeAvailable(GraphicLib)
    endif()

    #add dynamic lib
    add_library(GraphicLib SHARED IMPORTED)

    if(WIN32)
        set_target_properties(GraphicLib PROPERTIES
            IMPORTED_IMPLIB "${graphiclib_SOURCE_DIR}/${GRAPHIC_IMPLIB_NAME}"
        )
    endif()

    set_target_properties(GraphicLib PROPERTIES
         IMPORTED_LOCATION "${graphiclib_SOURCE_DIR}/${GRAPHIC_BIN_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${graphiclib_SOURCE_DIR}/include"
    )

    # Only add the vcpkg include path when vcpkg is actually used
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set_property(TARGET GraphicLib APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    endif()
endif()


#set variable to fetch the ECS part
set(ECS_URL "https://github.com/rtk-lib/ECS/releases/latest/download/rtk_ecs-1.04.tar.gz")
set(ECS_FOLDER_NAME "rtk_ecs/include")

#declare a fetch toward the ecs lib
FetchContent_Declare(
    ECSLib
    URL ${ECS_URL}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/ECSLib
)

#Get variable defined by the FetchContent_Declare
FetchContent_GetProperties(ECSLib)

#check if the lib is already dowload
if(NOT ecslib_POPULATED)
    FetchContent_MakeAvailable(ECSLib)
endif()

add_library(ECSLib INTERFACE)

target_include_directories(ECSLib INTERFACE "${ecslib_SOURCE_DIR}/include")

#Network
find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)

add_library(Network STATIC
    lib/Network/src/metrics/Metrics.cpp

    lib/Network/src/protocol/PacketReader.cpp
    lib/Network/src/protocol/PacketWriter.cpp
    lib/Network/src/protocol/PacketHeader.cpp
    lib/Network/src/protocol/HandshakeHeader.cpp
    lib/Network/src/protocol/MessageHeader.cpp
    lib/Network/src/protocol/MessageParser.cpp
    lib/Network/src/protocol/PingMessage.cpp
    lib/Network/src/protocol/PongMessage.cpp
    lib/Network/src/protocol/Capture.cpp

    lib/Network/src/tracking/PacketTracker.cpp
    lib/Network/src/connection/Connection.cpp
    lib/Network/src/transport/UdpTransport.cpp
    lib/Network/src/server/NetworkServer.cpp
)

target_include_directories(Network
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/Network/src
)

target_compile_features(Network
    PUBLIC
        cxx_std_20
)

target_compile_definitions(Network
    PUBLIC
        ASIO_STANDALONE
)

target_link_libraries(Network
    PUBLIC
        Threads::Threads
        asio::asio
)

#JSON LOADER
find_package(nlohmann_json CONFIG REQUIRED)

#Resources: reads assets from disk (dev builds)
add_library(Resources STATIC src/Common/Resources/Resources.cpp)
target_include_directories(Resources PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/Common")

#NetworkClient
add_library(NetworkClientLib STATIC
    src/Client/NetworkClient.cpp
)

target_include_directories(NetworkClientLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Client
)

target_compile_features(NetworkClientLib
    PUBLIC
        cxx_std_20
)

target_link_libraries(NetworkClientLib
    PUBLIC
        Network
        CommonProtocole
)

# Lua Scripting Engine (via vcpkg)
find_package(Lua REQUIRED)

#Game Engine Lib
add_library(GameEngine INTERFACE)
target_link_libraries(GameEngine INTERFACE nlohmann_json::nlohmann_json ${LUA_LIBRARIES})
target_include_directories(GameEngine INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/GameEngine/include"
    ${LUA_INCLUDE_DIR}
)

add_library(CommonProtocole INTERFACE)
target_include_directories(CommonProtocole  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/Common/)
target_link_libraries(CommonProtocole INTERFACE Network)

if(RTYPE_BUILD_CLIENT)
    #CLIENT
    file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Client/*.cpp")
    list(REMOVE_ITEM CLIENT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Client/NetworkClient.cpp"
    )
    add_executable(r-type_client ${CLIENT_SOURCES})
    target_include_directories(r-type_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/Client")

    target_link_libraries(r-type_client PRIVATE GraphicLib ECSLib GameEngine Vulkan::Vulkan Network CommonProtocole  NetworkClientLib Resources)

    if (APPLE)
        get_filename_component(VULKAN_LIB_DIR "${Vulkan_LIBRARY}" DIRECTORY)
        set_property(TARGET r-type_client APPEND PROPERTY
            BUILD_RPATH "${graphiclib_SOURCE_DIR}/lib" "${VULKAN_LIB_DIR}"
        )
    endif()
endif()

#SERVER
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Server/*.cpp")

add_executable(r-type_server ${SERVER_SOURCES})
target_include_directories(r-type_server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/Server")

target_link_libraries(r-type_server PRIVATE ECSLib GameEngine Network CommonProtocole Resources)

#EMBEDDED: same sources, but assets are linked in the binary
set(EMBEDDED_ASSETS_CPP ${CMAKE_BINARY_DIR}/generated/EmbeddedAssets.cpp)
file(GLOB_RECURSE EMBEDDED_ASSET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
list(FILTER EMBEDDED_ASSET_FILES EXCLUDE REGEX ":[^/]*$")

add_custom_command(
    OUTPUT ${EMBEDDED_ASSETS_CPP}
    COMMAND ${CMAKE_COMMAND}
        -DROOT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DASSETS_DIR=assets
        -DOUTPUT=${EMBEDDED_ASSETS_CPP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedAssets.cmake
    DEPENDS ${EMBEDDED_ASSET_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedAssets.cmake
    COMMENT "Embedding assets"
    VERBATIM
)

add_library(ResourcesEmbedded STATIC
    src/Common/Resources/Resources.cpp
    ${EMBEDDED_ASSETS_CPP}
)
target_include_directories(ResourcesEmbedded PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/Common")
target_compile_definitions(ResourcesEmbedded PUBLIC RTYPE_EMBED_ASSETS)

# the C++ runtime is linked statically too, so nothing but system libs is needed
function(rtype_static_runtime target)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
        target_link_options(${target} PRIVATE -static-libstdc++ -static-libgcc)
    endif()
endfunction()

add_executable(r-type_server_embedded EXCLUDE_FROM_ALL ${SERVER_SOURCES})
target_include_directories(r-type_server_embedded PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/Server")
target_link_libraries(r-type_server_embedded PRIVATE ECSLib GameEngine Network CommonProtocole ResourcesEmbedded)
rtype_static_runtime(r-type_server_embedded)

if(RTYPE_BUILD_CLIENT AND RTYPE_EMBEDDED)
    # the static build of the graphical lib, it does not carry its system deps
    FetchContent_Declare(
        GraphicLibStatic
        URL ${GRAPHIC_STATIC_URL}
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/External/GraphicLibStatic
    )
    FetchContent_GetProperties(GraphicLibStatic)
    if(NOT graphiclibstatic_POPULATED)
        FetchContent_MakeAvailable(GraphicLibStatic)
    endif()

    add_library(GraphicLibStatic STATIC IMPORTED)
    set_target_properties(GraphicLibStatic PROPERTIES
        IMPORTED_LOCATION "${graphiclibstatic_SOURCE_DIR}/${GRAPHIC_STATIC_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${graphiclibstatic_SOURCE_DIR}/include"
    )
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set_property(TARGET GraphicLibStatic APPEND PROPERTY
            INTERFACE_INCLUDE_DIRECTORIES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    endif()
    if(APPLE)
        set_property(TARGET GraphicLibStatic APPEND PROPERTY INTERFACE_LINK_LIBRARIES
            "-framework Cocoa" "-framework QuartzCore" "-framework CoreGraphics" "-framework Metal" "-framework GameController")
    elseif(WIN32)
        set_property(TARGET GraphicLibStatic APPEND PROPERTY INTERFACE_LINK_LIBRARIES user32 gdi32 xinput)
    else()
        find_package(X11 REQUIRED)
        set_property(TARGET GraphicLibStatic APPEND PROPERTY INTERFACE_LINK_LIBRARIES X11::X11)
    endif()

    add_executable(r-type_client_embedded EXCLUDE_FROM_ALL ${CLIENT_SOURCES})
    target_include_directories(r-type_client_embedded PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/Client")
    target_link_libraries(r-type_client_embedded PRIVATE GraphicLibStatic ECSLib GameEngine Vulkan::Vulkan Network CommonProtocole NetworkClientLib ResourcesEmbedded)
    rtype_static_runtime(r-type_client_embedded)
endif()

if(WIN32 AND RTYPE_BUILD_CLIENT)
    foreach(TARGET_NAME r-type_client r-type_server)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${graphiclib_SOURCE_DIR}/${GRAPHIC_BIN_NAME}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        )
    endforeach()
endif()

# Unit tests (cmake -DRTYPE_BUILD_TESTS=ON, or make tests), see tests/
option(RTYPE_BUILD_TESTS "Build the unit tests, run them with ctest" OFF)
if(RTYPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Compression benchmark (cmake -DRTYPE_BUILD_BENCH=ON), see tools/compression_bench
if(RTYPE_BUILD_BENCH)
    find_package(zstd CONFIG REQUIRED)
    find_package(lz4 CONFIG REQUIRED)
    find_package(unofficial-brotli CONFIG REQUIRED)
    find_package(LibLZMA REQUIRED)
    find_package(ZLIB REQUIRED)
    add_executable(compression_bench tools/compression_bench/main.cpp)
    target_link_libraries(compression_bench PRIVATE
        $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
        lz4::lz4
        unofficial::brotli::brotlienc
        unofficial::brotli::brotlidec
        LibLZMA::LibLZMA
        ZLIB::ZLIB)
endif()

    cmake_minimum_required(VERSION 3.20)

project(R-Type VERSION 0.1 LANGUAGES CXX)
#force CMP0135 new force a timestamps on fetch
cmake_policy(SET CMP0135 NEW)

#use c++ 20
set(CMAKE_CXX_STANDARD 20)
#force c++ 20
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#Output directory for executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
foreach(CONFIG_NAME DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endforeach()

if(MSVC AND VCPKG_TARGET_TRIPLET MATCHES "static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

option(RTYPE_BUILD_CLIENT "Build the client (needs Vulkan and GraphicLib)" ON)
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
        FetchContent_Populate(GraphicLib)
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
    FetchContent_Populate(ECSLib)
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
target_link_libraries(Resources PUBLIC nlohmann_json::nlohmann_json)

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

#Game Engine Lib
add_library(GameEngine INTERFACE)
target_link_libraries(GameEngine INTERFACE nlohmann_json::nlohmann_json)
target_include_directories(GameEngine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib/GameEngine/include")

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

add_custom_command(
    OUTPUT ${EMBEDDED_ASSETS_CPP}
    COMMAND ${CMAKE_COMMAND}
        -DROOT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DASSETS_DIR=assets
        -DOUTPUT=${EMBEDDED_ASSETS_CPP}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedAssets.cmake
    DEPENDS ${EMBEDDED_ASSET_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedAssets.cmake
    COMMENT "Embedding assets"
)

add_library(ResourcesEmbedded STATIC
    src/Common/Resources/Resources.cpp
    ${EMBEDDED_ASSETS_CPP}
)
target_include_directories(ResourcesEmbedded PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/Common")
target_compile_definitions(ResourcesEmbedded PUBLIC RTYPE_EMBED_ASSETS)
target_link_libraries(ResourcesEmbedded PUBLIC nlohmann_json::nlohmann_json)

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
        FetchContent_Populate(GraphicLibStatic)
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

# #Test LIB NETWORK
# include(CTest)

# if(BUILD_TESTING)

#     add_executable(test_tracking
#         tests/network/testTracking.cpp
#     )
#     target_link_libraries(test_tracking PRIVATE Network)

#     add_executable(test_connection
#         tests/network/testConnection.cpp
#     )
#     target_link_libraries(test_connection PRIVATE Network)

#     add_executable(test_message_header
#         tests/network/testMessageHeader.cpp
#     )
#     target_link_libraries(test_message_header PRIVATE Network)

#     add_executable(test_message_parser
#         tests/network/testMessageParser.cpp
#     )
#     target_link_libraries(test_message_parser PRIVATE Network)

#     add_executable(test_server_handshake
#         tests/network/testServerHandshake.cpp
#     )
#     target_link_libraries(test_server_handshake PRIVATE Network)

#     add_executable(test_connected_packet
#         tests/network/testConnectedPacket.cpp
#     )
#     target_link_libraries(test_connected_packet PRIVATE Network)


#     add_test(
#         NAME PacketTracking
#         COMMAND test_tracking
#     )

#     add_test(
#         NAME Connection
#         COMMAND test_connection
#     )

#     add_test(
#         NAME MessageHeader
#         COMMAND test_message_header
#     )

#     add_test(
#         NAME MessageParser
#         COMMAND test_message_parser
#     )

#     add_test(
#         NAME ServerHandshake
#         COMMAND test_server_handshake
#     )

#     add_test(
#         NAME ConnectedPacket
#         COMMAND test_connected_packet
#     )


#     set_tests_properties(
#         ServerHandshake
#         ConnectedPacket
#         PROPERTIES
#             RUN_SERIAL TRUE
#     )

# add_executable(test_network_client
#     tests/network/testNetworkClient.cpp
#     src/Server/Server.cpp
# )

# target_include_directories(test_network_client
#     PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}/src/Server
# )

# target_compile_features(test_network_client
#     PRIVATE
#         cxx_std_20
# )

# target_link_libraries(test_network_client
#     PRIVATE
#         NetworkClientLib
#         Network
#         CommonProtocole
# )

# add_test(
#     NAME NetworkClientIntegration
#     COMMAND test_network_client
# )

# set_tests_properties(
#     NetworkClientIntegration
#     PROPERTIES
#         RUN_SERIAL TRUE
#         TIMEOUT 15
# )

# set_target_properties(
#     test_network_client
#     PROPERTIES
#         RUNTIME_OUTPUT_DIRECTORY
#             "${CMAKE_BINARY_DIR}/tests"
# )

# add_executable(test_input_queue
#     tests/network/testInputQueue.cpp
#     src/Server/Server.cpp
# )

# target_include_directories(test_input_queue
#     PRIVATE
#         "${CMAKE_CURRENT_SOURCE_DIR}/src/Server"
# )

# target_compile_features(test_input_queue
#     PRIVATE
#         cxx_std_20
# )

# target_link_libraries(test_input_queue
#     PRIVATE
#         Network
#         CommonProtocole
# )

# set_target_properties(test_input_queue
#     PROPERTIES
#         RUNTIME_OUTPUT_DIRECTORY
#             "${CMAKE_BINARY_DIR}/tests"
# )

# add_test(
#     NAME InputQueue
#     COMMAND test_input_queue
# )

# set_tests_properties(
#     InputQueue
#     PROPERTIES
#         RUN_SERIAL TRUE
#         TIMEOUT 15
# )

# add_executable(test_replication_payload
#     tests/network/testReplicationPayload.cpp
# )

# target_compile_features(test_replication_payload
#     PRIVATE
#         cxx_std_20
# )

# target_link_libraries(test_replication_payload
#     PRIVATE
#         CommonProtocole
#         Network
# )

# set_target_properties(test_replication_payload
#     PROPERTIES
#         RUNTIME_OUTPUT_DIRECTORY
#             "${CMAKE_BINARY_DIR}/tests"
# )

# add_test(
#     NAME ReplicationPayload
#     COMMAND test_replication_payload
# )

# set_tests_properties(
#     ReplicationPayload
#     PROPERTIES
#         TIMEOUT 15
# )

# add_executable(test_replication_integration
#     tests/network/testReplicationIntegration.cpp
#     src/Server/Server.cpp
# )

# target_include_directories(test_replication_integration
#     PRIVATE
#         "${CMAKE_CURRENT_SOURCE_DIR}/src/Server"
# )

# target_compile_features(test_replication_integration
#     PRIVATE
#         cxx_std_20
# )

# target_link_libraries(test_replication_integration
#     PRIVATE
#         NetworkClientLib
#         Network
#         CommonProtocole
# )

# set_target_properties(test_replication_integration
#     PROPERTIES
#         RUNTIME_OUTPUT_DIRECTORY
#             "${CMAKE_BINARY_DIR}/tests"
# )

# add_test(
#     NAME ReplicationIntegration
#     COMMAND test_replication_integration
# )

# set_tests_properties(
#     ReplicationIntegration
#     PROPERTIES
#         RUN_SERIAL TRUE
#         TIMEOUT 15
# )

#     function(enable_test_assertions target)
#       target_compile_options(${target} PRIVATE -UNDEBUG)
#     endfunction()

#     enable_test_assertions(test_tracking)
#     enable_test_assertions(test_connection)
#     enable_test_assertions(test_message_header)
#     enable_test_assertions(test_message_parser)
#     enable_test_assertions(test_server_handshake)
#     enable_test_assertions(test_connected_packet)

# endif()

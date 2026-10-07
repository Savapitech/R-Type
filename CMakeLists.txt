    cmake_minimum_required(VERSION 3.20)

project(R-Type VERSION 0.1 LANGUAGES CXX)
#force CMP0135 new force a timestamps on fetch
cmake_policy(SET CMP0135 NEW)

#use c++ 20
set(CMAKE_CXX_STANDARD 20)
#force c++ 20
set(CMAKE_CXX_STANDARD_REQUIRED True)

#Output directory for executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
foreach(CONFIG_NAME DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/bin)
endforeach()

set(CMAKE_MACOSX_RPATH TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

find_package(Vulkan REQUIRED)

#load fetchContent module
include(FetchContent)

#set all the url and the bin name
if(WIN32)
    set(GRAPHIC_URL "https://github.com/rtk-lib/graphical/releases/latest/download/rtk_graphical-windows.zip")
    set(GRAPHIC_BIN_NAME "bin/rtk_graphical.dll")
    set(GRAPHIC_IMPLIB_NAME "lib/rtk_graphical.lib")
elseif(APPLE)
    set(GRAPHIC_URL "https://github.com/rtk-lib/graphical/releases/latest/download/rtk_graphical-macos.tar.gz")
    set(GRAPHIC_BIN_NAME "lib/librtk_graphical.dylib")
elseif(UNIX AND NOT APPLE)
    set(GRAPHIC_URL "https://github.com/rtk-lib/graphical/releases/latest/download/rtk_graphical-linux.tar.gz")
    set(GRAPHIC_BIN_NAME "lib/librtk_graphical.so")
endif()

#declare a fetch toward the graphical lib
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


#set variable to fetch the ECS part
set(ECS_URL "https://github.com/rtk-lib/ECS/releases/latest/download/rtk_ecs-v1.0.3.tar.gz")
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

#Game Engine Lib
add_library(GameEngine INTERFACE)
target_include_directories(GameEngine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/lib/GameEngine/include")

add_library(CommonProtocole INTERFACE)
target_include_directories(CommonProtocole  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/Common/)

#CLIENT
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Client/*.cpp")

add_executable(r-type_client ${CLIENT_SOURCES})

target_link_libraries(r-type_client PRIVATE GraphicLib ECSLib GameEngine Vulkan::Vulkan Network CommonProtocole)

if (APPLE)
    set_property(TARGET r-type_client APPEND PROPERTY
        BUILD_RPATH "${graphiclib_SOURCE_DIR}/lib"
    )
endif()

#SERVER
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/Server/*.cpp")

add_executable(r-type_server ${SERVER_SOURCES})

target_link_libraries(r-type_server PRIVATE ECSLib GameEngine Network CommonProtocole)

if(WIN32)
    foreach(TARGET_NAME r-type_client r-type_server)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${graphiclib_SOURCE_DIR}/${GRAPHIC_BIN_NAME}"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        )
    endforeach()
endif()

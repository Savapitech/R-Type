cmake_minimum_required(VERSION 3.20)

project(GameEngine VERSION 1.0 LANGUAGES CXX)
#force CMP0135 new force a timestamps on fetch
cmake_policy(SET CMP0135 NEW)

#use c++ 20
set(CMAKE_CXX_STANDARD 20)
#force c++ 20
set(CMAKE_CXX_STANDARD_REQUIRED True)

#load fetchContent module
include(FetchContent)

#set all the url and the bin name
if(WIN32)
    set(GRAPHIC_URL "https://github.com/rtk-lib/graphical/releases/latest/download/rtk_graphical-windows.zip")
    set(GRAPHIC_BIN_NAME "lib/rtk_graphical.dll")
elseif(APPLE)
    set(GRAPHIC_URL "https://github.com/rtk-lib/graphical/releases/latest/download/rtk_graphical-mac.tar.gz")
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

set_target_properties(GraphicLib PROPERTIES
    IMPORTED_LOCATION "${graphiclib_SOURCE_DIR}/${GRAPHIC_BIN_NAME}"
    INTERFACE_INCLUDE_DIRECTORIES "${graphiclib_SOURCE_DIR}/include"
)

#set variable to fetch the ECS part
set(ECS_URL "https://github.com/rtk-lib/ECS/releases/latest/download/rtk_ecs-v1.0.1.tar.gz")
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

target_include_directories(ECSLib INTERFACE "${ecslib_SOURCE_DIR}")

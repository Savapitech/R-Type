file(GLOB_RECURSE PONG_CLIENT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/client/*.cpp"
)
file(GLOB_RECURSE PONG_SERVER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/server/*.cpp"
)

# Client
add_executable(pong ${PONG_CLIENT_SOURCES})
target_include_directories(pong PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/client"
)
target_compile_definitions(pong PRIVATE ASIO_STANDALONE)
target_link_libraries(pong PRIVATE
    GraphicLib ECSLib GameEngine Vulkan::Vulkan asio::asio Threads::Threads
    ${CMAKE_DL_LIBS}
)

# Server
add_executable(pong_server ${PONG_SERVER_SOURCES})
target_include_directories(pong_server PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/server"
)
target_compile_definitions(pong_server PRIVATE ASIO_STANDALONE)
target_link_libraries(pong_server PRIVATE
    ECSLib GameEngine asio::asio Threads::Threads
)

# Audio plugins, loaded at run time from bin/plugins by the client
set(PONG_PLUGIN_DIR "${RTYPE_OUTPUT_DIR}/plugins")

add_library(pong_audio_rtk MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/audio_rtk/AudioRtk.cpp"
)
target_link_libraries(pong_audio_rtk PRIVATE GraphicLib GameEngine)

add_library(pong_audio_dummy MODULE
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/audio_dummy/AudioDummy.cpp"
)
target_link_libraries(pong_audio_dummy PRIVATE GameEngine)

foreach(PLUGIN pong_audio_rtk pong_audio_dummy)
    # the client looks for lib<name><extension> on every platform
    set_target_properties(${PLUGIN} PROPERTIES
        PREFIX "lib"
        LIBRARY_OUTPUT_DIRECTORY "${PONG_PLUGIN_DIR}"
    )
    if(APPLE)
        # CMake names modules .so on macOS, DynamicLibrary expects .dylib
        set_target_properties(${PLUGIN} PROPERTIES SUFFIX ".dylib")
    endif()
    foreach(CONFIG_NAME DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${PLUGIN} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${CONFIG_NAME} "${PONG_PLUGIN_DIR}"
        )
    endforeach()
endforeach()
add_dependencies(pong pong_audio_rtk pong_audio_dummy)

if(APPLE)
    get_filename_component(VULKAN_LIB_DIR "${Vulkan_LIBRARY}" DIRECTORY)
    foreach(TARGET_NAME pong pong_audio_rtk)
        set_property(TARGET ${TARGET_NAME} APPEND PROPERTY
            BUILD_RPATH "${graphiclib_SOURCE_DIR}/lib" "${VULKAN_LIB_DIR}"
        )
    endforeach()
endif()

if(WIN32)
    add_custom_command(TARGET pong POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${graphiclib_SOURCE_DIR}/${GRAPHIC_BIN_NAME}"
            "$<TARGET_FILE_DIR:pong>"
    )
endif()

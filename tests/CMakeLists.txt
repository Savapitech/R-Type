# Unit tests, built with -DRTYPE_BUILD_TESTS=ON (or `make tests`) and run with
# ctest. Every executable shares the runner of framework/, see Test.hpp.

add_custom_target(rtype_tests)

set(RTYPE_TESTS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tests)

# rtype_add_test(<name> <sources...>)
function(rtype_add_test name)
    add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/framework/Main.cpp ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/framework)
    target_compile_features(${name} PRIVATE cxx_std_20)
    set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${RTYPE_TESTS_OUTPUT_DIR})
    foreach(CONFIG_NAME DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CONFIG_NAME} ${RTYPE_TESTS_OUTPUT_DIR})
    endforeach()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
    add_dependencies(rtype_tests ${name})
endfunction()

# lib/Network: serialization, reliability layer, metrics and the server
file(GLOB NETWORK_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/network/*.cpp)
rtype_add_test(test_network ${NETWORK_TEST_SOURCES})
target_link_libraries(test_network PRIVATE Network)
# the server tests bind UDP ports on the loopback
set_tests_properties(test_network PROPERTIES RUN_SERIAL TRUE)

# lib/GameEngine: components, systems, factory, scenes and input mapping
file(GLOB ENGINE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp)
rtype_add_test(test_engine ${ENGINE_TEST_SOURCES})
target_link_libraries(test_engine PRIVATE ECSLib GameEngine)

# src/Common/Protocol: payloads shared by the client and the server
file(GLOB PROTOCOL_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocol/*.cpp)
rtype_add_test(test_protocol ${PROTOCOL_TEST_SOURCES})
target_link_libraries(test_protocol PRIVATE CommonProtocole)

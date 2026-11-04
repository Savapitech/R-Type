if(NOT GENERATOR OR NOT EXISTS "${BUILD_DIR}/CMakeCache.txt")
    return()
endif()

file(STRINGS "${BUILD_DIR}/CMakeCache.txt" CACHED_GENERATOR
    REGEX "^CMAKE_GENERATOR:INTERNAL=")
string(REPLACE "CMAKE_GENERATOR:INTERNAL=" "" CACHED_GENERATOR "${CACHED_GENERATOR}")

if(NOT CACHED_GENERATOR STREQUAL GENERATOR)
    message(STATUS "${BUILD_DIR}: generator changed (${CACHED_GENERATOR} -> ${GENERATOR}), resetting the cmake cache")
    file(REMOVE "${BUILD_DIR}/CMakeCache.txt")
    file(REMOVE_RECURSE "${BUILD_DIR}/CMakeFiles")
    file(GLOB SUBBUILDS "${BUILD_DIR}/_deps/*-subbuild")
    file(REMOVE_RECURSE ${SUBBUILDS})
endif()

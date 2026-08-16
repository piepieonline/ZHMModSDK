# This script is included from the mod template when ZHMMODSDK_DIR is set.
# Its purpose is to configure, build, and install the SDK with the current preset
# and then expose it for use as a target.
message(STATUS "Using local version of ZHMModSDK from \"${ZHMMODSDK_DIR}\".")

# A mod may set ZHMMODSDK_PRESET itself to build the SDK with a preset other than
# the native MSVC ones -- notably x64-Release-Cross / x64-Debug-Cross when
# cross-compiling from Linux, where the cl.exe-based presets cannot configure.
if (NOT DEFINED ZHMMODSDK_PRESET)
    set(ZHMMODSDK_PRESET "x64-Release")
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(ZHMMODSDK_PRESET "x64-Debug")
    endif()
endif()

# The ZHMMODSDK_DIST_DIR can override the location from which sdk-dist.cmake loads the SDK from.
set(ZHMMODSDK_DIST_DIR "${ZHMMODSDK_DIR}/_install/${ZHMMODSDK_PRESET}")
set(ZHMMODSDK_INCLUDE_DIR "${ZHMMODSDK_DIST_DIR}/include")
set(ZHMMODSDK_BUILD_DIR "${ZHMMODSDK_DIR}/_build/${ZHMMODSDK_PRESET}")

# While configuring the mod, CMake exports its own compiler selection as CC/CXX.
# The nested SDK build must not inherit those: when the mod is cross-compiled from
# Linux, CXX is clang-cl (targeting Windows), which the SDK's vcpkg would then pick
# as the *host* x64-linux compiler and fail to build any host tool. Each preset
# names its own compilers, so dropping these is safe on every platform.
set(ZHMMODSDK_CMAKE ${CMAKE_COMMAND} -E env --unset=CC --unset=CXX ${CMAKE_COMMAND})

# cmake --preset x64-Debug .
execute_process(
    COMMAND ${ZHMMODSDK_CMAKE} --preset ${ZHMMODSDK_PRESET} .
    WORKING_DIRECTORY ${ZHMMODSDK_DIR}
)

# cmake --build _build/x64-Debug --parallel
execute_process(
    COMMAND ${ZHMMODSDK_CMAKE} --build _build/${ZHMMODSDK_PRESET} --parallel --target install
    WORKING_DIRECTORY ${ZHMMODSDK_DIR}
)

# Copy sdk-dist.cmake to _install.
file(COPY "${ZHMMODSDK_DIR}/cmake/sdk-dist.cmake" DESTINATION "${ZHMMODSDK_DIST_DIR}")
file(RENAME "${ZHMMODSDK_DIST_DIR}/sdk-dist.cmake" "${ZHMMODSDK_DIST_DIR}/CMakeLists.txt")

# Add the built SDK as a subproject.
add_subdirectory("${ZHMMODSDK_DIST_DIR}" "${ZHMMODSDK_BUILD_DIR}")

# Create a build step to re-build the SDK and add it as a dependency to the SDK target.
add_custom_target(_zhmBuild
    COMMAND ${ZHMMODSDK_CMAKE} --build _build/${ZHMMODSDK_PRESET} --parallel --target install
    WORKING_DIRECTORY ${ZHMMODSDK_DIR}
    COMMENT "Building ZHMModSDK (${ZHMMODSDK_PRESET})..."
    BYPRODUCTS "${ZHMMODSDK_DIST_DIR}"
)

add_dependencies(ZHMModSDK _zhmBuild)

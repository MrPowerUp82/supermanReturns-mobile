cmake_minimum_required(VERSION 3.25)
project(superman_runtime_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(SR_SDK "${SR_ROOT}/.references/rexglue-sdk")
# Checks of runtime internals; no private PC sources or game data.
include_directories("${SR_SDK}/include" "${SR_SDK}/thirdparty/simde" "${SR_SDK}/thirdparty/fmt/include" "${SR_SDK}/thirdparty/spdlog/include" "${SR_ROOT}/.tools/game-build/rexglue-sdk/include" "${SR_SDK}/thirdparty/imgui" "${SR_SDK}/thirdparty/sdl3/include" "${SR_SDK}/thirdparty/xxHash")
add_compile_definitions(SPDLOG_FMT_EXTERNAL SPDLOG_COMPILED_LIB REX_HAS_VULKAN=1)

add_executable(test_proc_maps "${SR_ROOT}/tests/native/test_proc_maps.cpp")
target_include_directories(test_proc_maps PRIVATE "${SR_SDK}/src")
target_compile_options(test_proc_maps PRIVATE -Wall -Wextra)

set(SR_RUNTIME_LIBRARY "${SR_SDK}/out/linux-arm64/librexruntime.so" CACHE FILEPATH "Existing tested ARM64 runtime")
add_library(sr_runtime SHARED IMPORTED)
set_target_properties(sr_runtime PROPERTIES IMPORTED_LOCATION "${SR_RUNTIME_LIBRARY}")
add_executable(test_region_query "${SR_ROOT}/tests/native/test_region_query.cpp")
target_link_libraries(test_region_query PRIVATE sr_runtime log)

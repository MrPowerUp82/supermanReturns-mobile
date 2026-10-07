cmake_minimum_required(VERSION 3.25)
project(superman_android_game LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Android ARM64 target only")
endif()
set(SR_RUNTIME_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.references/rexglue-sdk" CACHE PATH "Pinned Android ReXGlue fork")
set(SR_GUEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.tools/android-guest" CACHE PATH "Private generated Superman code")
set(REXGLUE_USE_D3D12 OFF CACHE BOOL "" FORCE)
set(REXGLUE_USE_VULKAN ON CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_PERF_COUNTERS ON CACHE BOOL "" FORCE)
set(REXGLUE_ENABLE_FIDELITYFX OFF CACHE BOOL "" FORCE)
set(REXGLUE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_link_options(-Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)
add_subdirectory("${SR_RUNTIME_ROOT}" rexglue-sdk EXCLUDE_FROM_ALL)
include("${SR_GUEST_ROOT}/generated/default/sources.cmake")
add_library(superman_recomp OBJECT ${GENERATED_SOURCES})
target_link_libraries(superman_recomp PRIVATE rex::runtime)
target_include_directories(superman_recomp PRIVATE "${SR_GUEST_ROOT}/generated/default")
target_compile_options(superman_recomp PRIVATE -fno-strict-aliasing -fwrapv -Wno-unused-variable -Wno-unused-label)
target_precompile_headers(superman_recomp PRIVATE "${SR_GUEST_ROOT}/generated/default/superman_returns_pch.h")
set(SR_NATIVE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.tools/pc-native")
set(SR_NATIVE_SRC "${SR_NATIVE_ROOT}/port/src")
set(SR_VULKAN_HEADERS "${SR_RUNTIME_ROOT}/thirdparty/vulkan-headers/include" CACHE PATH "Pinned Vulkan headers" FORCE)
set(SR_VULKAN_DXC "${CMAKE_CURRENT_LIST_DIR}/../../../superman_returns_recomp/.tools/dxc/bin/x64/dxc.exe" CACHE FILEPATH "Host shader compiler")
add_subdirectory("${SR_NATIVE_SRC}/graphics/vulkan" native-vulkan)
file(GLOB SR_NATIVE_GUEST_SOURCES CONFIGURE_DEPENDS "${SR_NATIVE_SRC}/graphics/guest/*.cpp")
add_library(superman_game SHARED main.cpp hooks.cpp native_bootstrap.cpp
  "${SR_GUEST_ROOT}/xma_fixes.cpp" "${SR_GUEST_ROOT}/skip_intro.cpp"
  "${SR_NATIVE_SRC}/native_renderer/native_hooks.cpp"
  "${SR_NATIVE_SRC}/native_renderer/native_frontend_android.cpp"
  "${SR_NATIVE_SRC}/native_renderer/native_bridge_android.cpp"
  "${SR_NATIVE_SRC}/native_renderer/native_command_system.cpp"
  "${SR_NATIVE_SRC}/native_renderer/native_graphics_system_vulkan.cpp"
  "${SR_NATIVE_SRC}/native_renderer/guest_reads_android.cpp"
  "${SR_NATIVE_SRC}/native_renderer/pm4_mirror.cpp"
  "${SR_NATIVE_SRC}/native_renderer/shader_registry.cpp"
  "${SR_NATIVE_SRC}/graphics/vulkan/platform/android_provider.cpp"
  ${SR_NATIVE_GUEST_SOURCES} $<TARGET_OBJECTS:superman_recomp>)
file(READ "${SR_NATIVE_ROOT}/source-lock.json" SR_NATIVE_LOCK)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SR_NATIVE_ROOT}/source-lock.json")
string(JSON SR_NATIVE_REVISION GET "${SR_NATIVE_LOCK}" commit)
file(SHA256 "${SR_NATIVE_ROOT}/source-lock.json" SR_NATIVE_DIGEST)
target_compile_definitions(superman_game PRIVATE SR_HAS_NATIVE=1 SR_NATIVE_RENDERER_BUILD=1
  _LIBCPP_ENABLE_EXPERIMENTAL VK_USE_PLATFORM_ANDROID_KHR
  SR_NATIVE_SOURCE_REVISION="${SR_NATIVE_REVISION}" SR_NATIVE_SOURCE_DIGEST="${SR_NATIVE_DIGEST}")
target_include_directories(superman_game PRIVATE "${SR_NATIVE_SRC}/native_renderer" "${SR_RUNTIME_ROOT}/thirdparty/xxHash")
target_include_directories(superman_game PRIVATE "${SR_GUEST_ROOT}" "${SR_GUEST_ROOT}/generated/default")
target_link_libraries(superman_game PRIVATE rex::runtime sr_vulkan_core android log)
rexglue_configure_target(superman_game)
target_link_options(superman_game PRIVATE -Wl,--no-undefined)




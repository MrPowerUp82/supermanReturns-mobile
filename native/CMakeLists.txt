cmake_minimum_required(VERSION 3.22)
project(superman_mobile LANGUAGES CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This target requires the Android NDK and arm64-v8a")
endif()
add_library(superman_mobile SHARED android_bridge.cpp
    vulkan/loader.cpp vulkan/policy.cpp vulkan/context.cpp
    vulkan/swapchain.cpp vulkan/frame_loop.cpp)
target_compile_features(superman_mobile PRIVATE cxx_std_20)
target_compile_definitions(superman_mobile PRIVATE VK_NO_PROTOTYPES VK_USE_PLATFORM_ANDROID_KHR)
target_compile_options(superman_mobile PRIVATE -Wall -Wextra -Werror=return-type)
target_link_options(superman_mobile PRIVATE -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)
target_link_libraries(superman_mobile PRIVATE android log dl)

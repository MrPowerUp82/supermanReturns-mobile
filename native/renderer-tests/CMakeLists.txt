cmake_minimum_required(VERSION 3.25)
project(superman_native_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(SR_SDK "${SR_ROOT}/.references/rexglue-sdk")
set(SR_PC "${SR_ROOT}/.tools/pc-native/port/src")
include_directories("${SR_SDK}/include" "${SR_SDK}/thirdparty/simde" "${SR_SDK}/thirdparty/fmt/include" "${SR_SDK}/thirdparty/spdlog/include" "${SR_ROOT}/.tools/game-build/rexglue-sdk/include" "${SR_SDK}/thirdparty/imgui" "${SR_SDK}/thirdparty/sdl3/include" "${SR_SDK}/thirdparty/xxHash")
add_compile_definitions(SPDLOG_FMT_EXTERNAL SPDLOG_COMPILED_LIB SR_HAS_NATIVE=1 SR_NATIVE_RENDERER_BUILD=1 REX_HAS_VULKAN=1)
add_executable(test_graphics_contract "${SR_ROOT}/tests/native/test_graphics_contract.cpp")
target_include_directories(test_graphics_contract PRIVATE "${SR_SDK}/include")
enable_testing()
add_test(NAME graphics_contract COMMAND test_graphics_contract)
add_library(test_frontend_packets_object OBJECT "${SR_ROOT}/tests/native/test_frontend_packets.cpp")
target_include_directories(test_frontend_packets_object PRIVATE "${SR_ROOT}/.tools/pc-native/port/src/native_renderer")
add_library(test_guest_reads_object OBJECT "${SR_ROOT}/tests/native/test_guest_reads.cpp")
target_include_directories(test_guest_reads_object PRIVATE "${SR_ROOT}/.tools/pc-native/port/src/native_renderer" "${SR_SDK}/include")
add_library(native_frontend_compile OBJECT "${SR_PC}/native_renderer/native_frontend_android.cpp" "${SR_PC}/native_renderer/guest_reads_android.cpp")
file(GLOB SR_GUEST_SOURCES "${SR_PC}/graphics/guest/*.cpp")
add_library(native_packet_capture STATIC
  ${SR_GUEST_SOURCES}
  "${SR_PC}/native_renderer/pm4_mirror.cpp"
  "${SR_PC}/native_renderer/shader_registry.cpp"
  $<TARGET_OBJECTS:native_frontend_compile>)
target_compile_options(native_packet_capture PRIVATE -ffunction-sections -fdata-sections)
add_executable(test_frontend_packets $<TARGET_OBJECTS:test_frontend_packets_object> "${SR_ROOT}/tests/native/runtime_test_support.cpp")
set(SR_RUNTIME_LIBRARY "${SR_SDK}/out/linux-arm64/librexruntime.so" CACHE FILEPATH "Existing tested ARM64 runtime")
add_library(sr_runtime SHARED IMPORTED)
set_target_properties(sr_runtime PROPERTIES IMPORTED_LOCATION "${SR_RUNTIME_LIBRARY}")
target_link_libraries(test_frontend_packets PRIVATE native_packet_capture sr_runtime log)
target_link_options(test_frontend_packets PRIVATE -Wl,--gc-sections)
add_executable(test_guest_reads $<TARGET_OBJECTS:test_guest_reads_object> "${SR_PC}/native_renderer/guest_reads_android.cpp")
target_link_libraries(test_guest_reads PRIVATE sr_runtime log)
add_test(NAME frontend_packets COMMAND test_frontend_packets)
add_test(NAME guest_reads COMMAND test_guest_reads)
add_executable(test_android_shader_library "${SR_ROOT}/tests/native/test_android_shader_library.cpp" "${SR_PC}/graphics/shaders/vulkan_shader_service.cpp")
target_include_directories(test_android_shader_library PRIVATE "${SR_PC}/graphics/shaders")
add_test(NAME android_shader_library COMMAND test_android_shader_library)

if(ANDROID)
  target_compile_definitions(test_android_shader_library PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL)
endif()
add_library(test_native_commands_object OBJECT "${SR_ROOT}/tests/native/test_native_commands.cpp")
target_include_directories(test_native_commands_object PRIVATE "${SR_PC}/native_renderer")
add_executable(test_native_commands $<TARGET_OBJECTS:test_native_commands_object> "${SR_PC}/native_renderer/native_command_system.cpp" "${SR_PC}/native_renderer/guest_reads_android.cpp" "${SR_ROOT}/tests/native/runtime_test_support.cpp")
target_sources(test_native_commands PRIVATE "${SR_PC}/native_renderer/native_graphics_system_vulkan.cpp" "${SR_PC}/native_renderer/native_bridge_android.cpp")
target_link_libraries(test_native_commands PRIVATE native_packet_capture sr_runtime log)
target_include_directories(test_native_commands PRIVATE "${SR_PC}/native_renderer")
target_link_options(test_native_commands PRIVATE -Wl,--gc-sections)
set(SR_VULKAN_HEADERS "${SR_SDK}/thirdparty/vulkan-headers/include" CACHE PATH "Pinned Vulkan headers" FORCE)
set(SR_VULKAN_DXC "${SR_ROOT}/../superman_returns_recomp/.tools/dxc/bin/x64/dxc.exe" CACHE FILEPATH "Host shader compiler")
add_subdirectory("${SR_PC}/graphics/vulkan" vulkan-core)
add_executable(test_native_provider "${SR_ROOT}/tests/native/test_native_provider.cpp" "${SR_PC}/graphics/vulkan/platform/android_provider.cpp" "${SR_ROOT}/tests/native/runtime_test_support.cpp")
target_include_directories(test_native_provider PRIVATE "${SR_PC}/native_renderer")
target_compile_definitions(test_native_provider PRIVATE _LIBCPP_ENABLE_EXPERIMENTAL VK_USE_PLATFORM_ANDROID_KHR)
target_link_libraries(test_native_provider PRIVATE sr_vulkan_core native_packet_capture sr_runtime android log)
target_link_options(test_native_provider PRIVATE -Wl,--gc-sections)

set(SR_VULKAN_TESTS "${SR_ROOT}/.tools/pc-native/tests/vulkan")
add_executable(test_vulkan_regressions
 "${SR_VULKAN_TESTS}/main.cpp"
 "${SR_VULKAN_TESTS}/test_policy.cpp"
 "${SR_VULKAN_TESTS}/test_context.cpp"
 "${SR_VULKAN_TESTS}/test_frame_loop.cpp"
 "${SR_VULKAN_TESTS}/test_swapchain.cpp"
 "${SR_VULKAN_TESTS}/test_triangle.cpp"
 "${SR_VULKAN_TESTS}/test_smoke_options.cpp"
 "${SR_VULKAN_TESTS}/test_device_requirements.cpp"
 "${SR_VULKAN_TESTS}/test_descriptors.cpp"
 "${SR_VULKAN_TESTS}/test_resources.cpp"
 "${SR_VULKAN_TESTS}/test_render_targets.cpp"
 "${SR_VULKAN_TESTS}/test_image_state.cpp"
 "${SR_VULKAN_TESTS}/test_shader_service.cpp"
 "${SR_VULKAN_TESTS}/test_game_pipeline.cpp"
 "${SR_VULKAN_TESTS}/test_resolve.cpp"
 "${SR_VULKAN_TESTS}/test_composition.cpp"
 "${SR_VULKAN_TESTS}/test_state_shadow.cpp"
)
target_include_directories(test_vulkan_regressions PRIVATE "${SR_ROOT}/.tools/pc-native/tests/native")
target_link_libraries(test_vulkan_regressions PRIVATE sr_vulkan_core)
add_library(test_native_bootstrap_object OBJECT "${SR_ROOT}/tests/native/test_native_bootstrap.cpp")
target_include_directories(test_native_bootstrap_object PRIVATE "${SR_ROOT}/native/game" "${SR_PC}/native_renderer")

add_executable(test_native_bootstrap $<TARGET_OBJECTS:test_native_bootstrap_object>
 "${SR_ROOT}/native/game/native_bootstrap.cpp"
 "${SR_PC}/native_renderer/native_command_system.cpp"
 "${SR_PC}/native_renderer/native_graphics_system_vulkan.cpp"
 "${SR_PC}/native_renderer/native_bridge_android.cpp"
 "${SR_PC}/graphics/vulkan/platform/android_provider.cpp")
target_include_directories(test_native_bootstrap PRIVATE "${SR_ROOT}/native/game" "${SR_PC}/native_renderer")
target_link_libraries(test_native_bootstrap PRIVATE sr_vulkan_core native_packet_capture sr_runtime android log)
target_link_options(test_native_bootstrap PRIVATE -Wl,--gc-sections)

add_executable(test_float_filter_gpu "${SR_ROOT}/tests/native/test_float_filter_gpu.cpp")
target_link_libraries(test_float_filter_gpu PRIVATE sr_vulkan_core)
add_executable(test_texture_filter "${SR_ROOT}/tests/native/test_texture_filter.cpp")
target_link_libraries(test_texture_filter PRIVATE sr_vulkan_core)
set(SR_FILTER_PROBE "${CMAKE_CURRENT_BINARY_DIR}/float_filter_probe.spv")
add_custom_command(OUTPUT "${SR_FILTER_PROBE}"
 COMMAND "${SR_VULKAN_DXC}" -spirv -fspv-target-env=vulkan1.1 -T cs_6_0 -E CSMain
 -I "${SR_ROOT}/native/shaders" -Fo "${SR_FILTER_PROBE}"
 "${SR_ROOT}/tests/native/shaders/float_filter_probe.hlsl"
 DEPENDS "${SR_ROOT}/native/shaders/float_filter.hlsl" "${SR_ROOT}/tests/native/shaders/float_filter_probe.hlsl"
 VERBATIM)
add_custom_target(filter_probe_shader DEPENDS "${SR_FILTER_PROBE}")
foreach(stage IN ITEMS vs ps)
 if(stage STREQUAL "vs")
  set(filter_entry VSMain)
 else()
  set(filter_entry PSMain)
 endif()
 set(probe "${CMAKE_CURRENT_BINARY_DIR}/float_filter_probe.${stage}.spv")
 add_custom_command(OUTPUT "${probe}"
  COMMAND "${SR_VULKAN_DXC}" -spirv -fspv-target-env=vulkan1.1 -T ${stage}_6_0 -E ${filter_entry}
  -I "${SR_ROOT}/native/shaders" -Fo "${probe}"
  "${SR_ROOT}/tests/native/shaders/float_filter_probe.hlsl"
  DEPENDS "${SR_ROOT}/native/shaders/float_filter.hlsl" "${SR_ROOT}/tests/native/shaders/float_filter_probe.hlsl"
  VERBATIM)
 add_custom_target(filter_probe_${stage} DEPENDS "${probe}")
endforeach()

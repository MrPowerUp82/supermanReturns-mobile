# Vulkan Android Backend Integration

## 1. Context and Purpose
The native renderer Vulkan engine on PC uses graphics/vulkan/platform/native_provider.cpp and shader_process.cpp to integrate with the OS. These files are tightly coupled to Windows (HWND, VK_KHR_win32_surface, CreateProcessW for dxc.exe). 
To render the game on Android, we need to bypass these Windows-specific platform files and provide an Android-compatible Vulkan integration layer.

## 2. Architecture and Approach
We will isolate the platform-specific code out of the generic Vulkan engine and provide Android equivalents.

### 2.1 Android Vulkan Provider (ndroid_provider.cpp)
- Create an Android-specific graphics provider for Vulkan that implements ex::ui::GraphicsProvider and ex::ui::Presenter.
- Instead of VK_KHR_win32_surface, it will use VK_KHR_android_surface (via kCreateAndroidSurfaceKHR).
- It will interface with Android's ANativeWindow.
- To avoid massive duplication, we will abstract the common Host and FrameLoop logic from 
ative_provider.cpp or simply create a dedicated ndroid_provider.cpp tailored for Android's windowing lifecycle.

### 2.2 Offline Shader Compilation (AndroidShaderProcess)
- Android devices cannot spawn dxc.exe. 
- For the initial port phase, AndroidShaderProcess will be a null implementation that always returns an error if a shader is not found in the precompiled cache.
- The game will rely entirely on a pre-bundled .vkshader library cache generated on PC.

## 3. Components Touched
1. superman_returns_recomp/port/src/graphics/vulkan/platform/android_provider.cpp (New)
2. superman_returns_recomp/port/src/graphics/vulkan/platform/shader_process_android.cpp (New)
3. superman_returns_recomp/port/src/native_renderer/native_graphics_system_vulkan.cpp (Update to use the new Android provider instead of stubs).
4. 
ative/game/CMakeLists.txt (Include the new files for the Android build).

## 4. Testing and Success Criteria
- The Android project compiles successfully with the full Vulkan runtime (no missing symbols for the NativeProvider).
- The ulkan_provider successfully initializes a Vulkan instance and VK_KHR_android_surface when run on the device.

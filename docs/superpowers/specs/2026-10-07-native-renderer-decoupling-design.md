# Native Renderer Platform Decoupling

## 1. Context and Purpose
The Superman Returns PC port features a highly optimized `native_renderer` built for Windows and Direct3D 12. Currently, the Android port completely bypasses this directory because it is tightly coupled to Windows headers (`COM`, `CreateProcessW`) and D3D12 types. As a result, the Android build falls back to the generic ReXGlue GPU emulator, which performs poorly and lacks game-specific optimizations.

This design outlines the structural refactoring needed to decouple the `native_renderer` from Windows/D3D12, creating an abstract foundation that allows the Android Vulkan implementation to be built side-by-side without breaking the existing PC D3D12 implementation.

## 2. Architecture and Approach
We will use an Interface/Implementation decoupling pattern (Approach A). The core game rendering logic will communicate with a platform-agnostic graphics system interface.

### 2.1 Interface Extraction (`INativeGraphicsSystem`)
- Extract a clean, platform-agnostic interface (`INativeGraphicsSystem` or similar) from the current `NativeGraphicsSystem`.
- Move all Windows-specific includes (`<wrl>`, `<d3d12.h>`, etc.) out of the common headers and strictly into the D3D12 implementation files.

### 2.2 Backend Implementations
- **D3D12 Backend:** Rename the existing implementation to `D3D12NativeGraphicsSystem` (or keep it in a D3D12-specific `.cpp` file compiled only on Windows).
- **Vulkan Backend (Android):** Create a new `VulkanNativeGraphicsSystem` skeleton. In this first structural step, it will contain stubs sufficient to compile on Android and integrate with the existing Android Vulkan runtime/Swapchain.

### 2.3 Build System Adjustments
- **PC `port/CMakeLists.txt`:** Wrap D3D12-specific sources in `if(WIN32)` checks.
- **Android `native/game/CMakeLists.txt`:** Include the `native_renderer` directory in the build, filtering out the Windows/D3D12 source files and compiling the new Vulkan/Android files.

## 3. Components Touched
1. `superman_returns_recomp/port/src/native_renderer/native_renderer.h`
   - Remove Windows/COM dependencies from the public structures.
   - Abstract the capture/replay state if it relies on D3D12 objects.
2. `superman_returns_recomp/port/src/native_renderer/native_graphics_system.h` & `.cpp`
   - Split into `INativeGraphicsSystem` (header) and backend-specific cpp files (e.g., `native_graphics_system_d3d12.cpp`, `native_graphics_system_vulkan.cpp`).
3. `superman_returns_recomp/port/CMakeLists.txt`
   - Conditional compilation based on target OS/Backend.
4. `native/game/CMakeLists.txt` (Android port)
   - Add the `native_renderer` to the Android build target.

## 4. Testing and Success Criteria
- **PC Regression:** The PC build compiles successfully on Windows and runs using D3D12 with zero regressions in rendering logic.
- **Android Compilation:** The `native_game` library compiles successfully on Android (`aarch64-linux-android`) including the `native_renderer` files, without complaining about missing Windows headers or COM types.
- **Runtime:** The game on Android initializes the new Vulkan skeleton without crashing, proving the interface injection works.

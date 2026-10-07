# Native Renderer Decoupling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decouple the PC `native_renderer` from Windows/D3D12 to allow its inclusion in the Android Vulkan build.

**Architecture:** Introduce `INativeGraphicsSystem` to abstract backend operations. Move all Windows-specific code into a `D3D12NativeGraphicsSystem` implementation. Create a stub `VulkanNativeGraphicsSystem` for Android.

**Tech Stack:** C++23, CMake, ReXGlue SDK (Vulkan/D3D12).

**Spec:** `docs/superpowers/specs/2026-10-07-native-renderer-decoupling-design.md`

## Global Constraints

- No `<windows.h>`, `<wrl>`, or `<d3d12.h>` includes allowed in public `.h` files within `native_renderer`.
- All C++ code must comply with C++23.

## Review Focus

- The PC game compilation breaks because `native_renderer.h` still leaks a COM pointer to the engine. (Add compilation check step in Task 1 and 2).
- The Android compilation breaks because it attempts to compile `native_graphics_system_d3d12.cpp`. (Add strict CMake filtering check in Task 4).

---

### Task 1: Extract `INativeGraphicsSystem` Interface

**Files:**
- Create: `superman_returns_recomp/port/src/native_renderer/native_graphics_system_interface.h`
- Modify: `superman_returns_recomp/port/src/native_renderer/native_graphics_system.h`

**Interfaces:**
- Produces: `class INativeGraphicsSystem` with virtual methods for the guest GPU contract (e.g., `Initialize`, `Shutdown`, `Swap`).

- [ ] **Step 1: Write the interface definition**
Define `INativeGraphicsSystem` with pure virtual equivalents of the current `NativeGraphicsSystem` public methods.

- [ ] **Step 2: Modify existing `NativeGraphicsSystem`**
Rename it to `D3D12NativeGraphicsSystem` and make it inherit from `INativeGraphicsSystem`. Keep all Windows headers isolated in this specific header/cpp pair (or move headers to the cpp).

- [ ] **Step 3: Verify PC Compilation (Fails due to main app usage)**
Run: `powershell -c "cd superman_returns_recomp/port/build; cmake --build ."`
Expected: FAIL (because `superman_returns_app.h` and others still refer to the old `NativeGraphicsSystem`).

- [ ] **Step 4: Commit**
```bash
git add superman_returns_recomp/port/src/native_renderer/
git commit -m "refactor: extract INativeGraphicsSystem interface"
```

---

### Task 2: Isolate D3D12 and Fix PC Consumers

**Files:**
- Modify: `superman_returns_recomp/port/src/superman_returns_app.h`
- Modify: `superman_returns_recomp/port/src/superman_returns_app.cpp`
- Modify: `superman_returns_recomp/port/src/native_renderer/native_renderer.h`

**Interfaces:**
- Consumes: `INativeGraphicsSystem` (from Task 1)

- [ ] **Step 1: Refactor `superman_returns_app.cpp` to use interface**
Change instantiations of the graphics system to return `unique_ptr<INativeGraphicsSystem>`, instantiating `D3D12NativeGraphicsSystem` conditionally on `#ifdef WIN32`.

- [ ] **Step 2: Clean `native_renderer.h` of Windows types**
Ensure `native_renderer.h` doesn't include `<d3d12.h>` or use `Microsoft::WRL::ComPtr` in its public struct definitions unless hidden behind an opaque pointer or `WIN32` guard.

- [ ] **Step 3: Verify PC Compilation Passes**
Run: `powershell -c "cd superman_returns_recomp/port; mkdir build -Force; cd build; cmake ..; cmake --build ."`
Expected: PASS (Successfully compiles the PC port).

- [ ] **Step 4: Commit**
```bash
git add superman_returns_recomp/port/src/
git commit -m "fix: isolate D3D12 and fix PC consumers"
```

---

### Task 3: Adjust PC CMakeLists

**Files:**
- Modify: `superman_returns_recomp/port/CMakeLists.txt`

**Interfaces:**
- Produces: Correct source inclusion based on target platform.

- [ ] **Step 1: Update CMakeLists.txt for native_renderer**
Group D3D12 specific files (`native_graphics_system_d3d12.cpp`, `shader_process.cpp`, etc.) under `if(WIN32)`.

- [ ] **Step 2: Verify PC Compilation Passes**
Run: `powershell -c "cd superman_returns_recomp/port/build; cmake --build ."`
Expected: PASS

- [ ] **Step 3: Commit**
```bash
git add superman_returns_recomp/port/CMakeLists.txt
git commit -m "build: adjust PC CMakeLists for D3D12 isolation"
```

---

### Task 4: Create Vulkan Stub and Android CMake Integration

**Files:**
- Create: `superman_returns_recomp/port/src/native_renderer/native_graphics_system_vulkan.h`
- Create: `superman_returns_recomp/port/src/native_renderer/native_graphics_system_vulkan.cpp`
- Modify: `native/game/CMakeLists.txt` (Android)

**Interfaces:**
- Consumes: `INativeGraphicsSystem`
- Produces: A compiling Android target containing the `native_renderer` module.

- [ ] **Step 1: Implement `VulkanNativeGraphicsSystem` stub**
Create the class inheriting from `INativeGraphicsSystem` with empty stubs (printing a log for now). 

- [ ] **Step 2: Integrate into Android `CMakeLists.txt`**
Modify `native/game/CMakeLists.txt` to include `native_renderer.cpp` and `native_graphics_system_vulkan.cpp`. Ensure `native_graphics_system_d3d12.cpp` and `shader_process.cpp` are explicitly excluded from this build.

- [ ] **Step 3: Verify Android Compilation Passes**
Run: `powershell -c ".\tools\build.ps1"`
Expected: PASS (APK builds successfully without missing Windows header errors).

- [ ] **Step 4: Commit**
```bash
git add superman_returns_recomp/port/src/native_renderer/native_graphics_system_vulkan.* native/game/CMakeLists.txt
git commit -m "feat: add Vulkan stub and integrate native_renderer into Android build"
```

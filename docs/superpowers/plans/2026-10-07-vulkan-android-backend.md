# Vulkan Android Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide an Android-compatible Vulkan provider and offline shader fallback to enable the native renderer on Android.

**Architecture:** Create an `AndroidShaderProcess` (stub) and an `android_provider.cpp` that implements `rex::ui::GraphicsProvider` using `VK_KHR_android_surface`.

**Tech Stack:** C++23, Vulkan, Android NDK (`ANativeWindow`).

**Spec:** `docs/superpowers/specs/2026-10-07-vulkan-android-backend-design.md`

## Global Constraints

- Android specific files must not include Windows headers.
- The project must compile successfully on Android.

## Review Focus

- Missing symbols for `ShaderProcess` or `GraphicsProvider` during Android link phase.
- Ensure the Android provider uses `VK_KHR_android_surface` correctly.

---

### Task 1: Create Android Shader Process Stub

**Files:**
- Create: `superman_returns_recomp/port/src/graphics/vulkan/platform/shader_process_android.h`
- Create: `superman_returns_recomp/port/src/graphics/vulkan/platform/shader_process_android.cpp`

**Interfaces:**
- Produces: `shaders::ShaderProcess AndroidShaderProcess()`

- [ ] **Step 1: Write `shader_process_android.h` and `.cpp`**
Create a function `AndroidShaderProcess()` returning a lambda that always sets `error = "Android cannot compile shaders offline; use precompiled cache."` and returns `false`.

- [ ] **Step 2: Commit**
```bash
git add port/src/graphics/vulkan/platform/shader_process_android.*
git commit -m "feat: add Android shader process stub"
```

---

### Task 2: Create Android Vulkan Provider

**Files:**
- Create: `superman_returns_recomp/port/src/graphics/vulkan/platform/android_provider.cpp`

**Interfaces:**
- Consumes: `AndroidShaderProcess` from Task 1.
- Produces: `rex::ui::GraphicsProvider` implementation for Android using `ANativeWindow`.

- [ ] **Step 1: Write `android_provider.cpp`**
Create an Android-specific provider that uses `vkCreateAndroidSurfaceKHR`. It will mimic the structure of `native_provider.cpp` but strip out `HWND` and `Win32HwndSurface`. 

- [ ] **Step 2: Commit**
```bash
git add port/src/graphics/vulkan/platform/android_provider.cpp
git commit -m "feat: add Android Vulkan provider"
```

---

### Task 3: Integrate into Android CMakeLists and Native Renderer

**Files:**
- Modify: `superman_returns_recomp/port/src/native_renderer/native_graphics_system_vulkan.cpp`
- Modify: `native/game/CMakeLists.txt`

**Interfaces:**
- Consumes: The newly created `android_provider.cpp`

- [ ] **Step 1: Update Android `CMakeLists.txt`**
Add `"../../superman_returns_recomp/port/src/graphics/vulkan/platform/shader_process_android.cpp"` and `"../../superman_returns_recomp/port/src/graphics/vulkan/platform/android_provider.cpp"` to the `superman_game` target.

- [ ] **Step 2: Verify Android Compilation**
Run: `powershell -c ".\tools\build.ps1"`
Expected: PASS (APK builds).

- [ ] **Step 3: Commit**
```bash
git add native/game/CMakeLists.txt port/src/native_renderer/native_graphics_system_vulkan.cpp
git commit -m "build: integrate Android Vulkan provider into build"
```

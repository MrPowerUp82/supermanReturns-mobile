# Decouple AVX2 Hashing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decouple `xxh3_avx2` usage in `native_renderer.cpp` so it can compile on ARM64.

**Architecture:** Use preprocessor macros to conditionally compile AVX2 calls.

**Tech Stack:** C++23.

**Spec:** `docs/superpowers/specs/2026-10-07-avx2-decoupling-design.md`

## Global Constraints
- `native_renderer.cpp` must compile on ARM64.

## Review Focus
- Ensure the AVX2 check logic does not accidentally disable AVX2 on PC MSVC where it is supported.

---

### Task 1: Conditionalize AVX2 Usage

**Files:**
- Modify: `superman_returns_recomp/port/src/native_renderer/native_renderer.cpp`

**Interfaces:**
- Consumes: N/A
- Produces: A platform-agnostic `TextureHash()` function.

- [ ] **Step 1: Wrap the header inclusion**
Wrap `#include "xxh3_avx2.h"` in `#if defined(__AVX2__) || defined(__x86_64__) || defined(_M_X64)`.

- [ ] **Step 2: Wrap the AVX2 logic inside `TextureHash`**
Inside `TextureHash()`, conditionally compile the AVX2 path:
```cpp
#if defined(__AVX2__) || defined(__x86_64__) || defined(_M_X64)
    const bool avx2 = superman_returns::native::Avx2Available() && !(off && off[0] && off[0]!='0');
    return avx2 ? &superman_returns::native::Xxh3Avx2 : &Xxh3Baseline;
#else
    return &Xxh3Baseline;
#endif
```

- [ ] **Step 3: Commit**
```bash
git add port/src/native_renderer/native_renderer.cpp
git commit -m "refactor: decouple AVX2 hashing for ARM64 compatibility"
```

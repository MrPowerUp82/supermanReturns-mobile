# Decouple AVX2 Hashing from Native Renderer

## 1. Context and Purpose
The PC port relies on 
ative_renderer/xxh3_avx2.cpp for fast texture and buffer hashing using the AVX2 instruction set. This optimization is strictly for x86_64 architecture and breaks compilation and linking on ARM64 (Android). To prepare 
ative_renderer.cpp for integration into the Android build, we must decouple this hardcoded dependency.

## 2. Architecture and Approach
We will wrap the inclusion and usage of Xxh3Avx2 in 
ative_renderer.cpp with compiler feature macros (e.g., #if defined(__AVX2__) or similar x86/MSVC checks). On platforms where AVX2 is unavailable (like ARM64), the code will fallback entirely to Xxh3Baseline without attempting to link against Xxh3Avx2.

## 3. Components Touched
1. superman_returns_recomp/port/src/native_renderer/native_renderer.cpp
   - Wrap #include "xxh3_avx2.h" in #if defined(__AVX2__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))).
   - Wrap the vx2 capability check and the return of &superman_returns::native::Xxh3Avx2 in the same macro. If the macro is not defined, always return &Xxh3Baseline.

## 4. Testing and Success Criteria
- The PC build continues to function and utilize the AVX2 path on supported x86/64 processors.
- The 
ative_renderer.cpp file can be successfully compiled for ARM64 without complaining about missing AVX2 instructions or unresolved Xxh3Avx2 symbols.

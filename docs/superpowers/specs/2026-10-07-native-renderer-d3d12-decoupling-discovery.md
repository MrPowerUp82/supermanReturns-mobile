# Decouple Native Renderer from D3D12 Types

## 1. Context and Purpose
The PC port relies on 
ative_renderer.h for its native graphics backend, which is fundamentally coupled to D3D12 (ComPtr, ID3D12Resource, DXGI_FORMAT, etc.). For Android, we need to bring the state tracking (PM4 parsing, texture hashes, etc.) without bringing in D3D12 or windows.h.

## 2. Analysis of Current State
Currently, 
ative_renderer.h contains class Renderer which holds both:
1. **Frontend / Capture State:** Pm4Mirror, CapturedTextureEntry, texture hashes, write sequences, ring constant arrays.
2. **Backend / Execution State:** ID3D12Device, ID3D12CommandQueue, ID3D12PipelineState.

The Android port fails to compile 
ative_renderer.cpp (and 	exture_decode.cpp, etc.) because the ReXGlue d3d12_api.h header includes <windows.h>, which fails on Android/Linux. 

## 3. Options for Structural Change
1. **Pimpl Idiom (Pointer to Implementation):** 
   - Move all D3D12 members of Renderer into a struct D3D12Impl; defined entirely in 
ative_renderer.cpp.
   - Result: 
ative_renderer.h requires no D3D12 headers, but 
ative_renderer.cpp still does (and thus won't compile on Android without #ifdef WIN32 wraps).
2. **Abstract Interface (IRendererBackend):** 
   - Create an interface for execution (Draw, Copy, etc).
   - Move PM4 parsing and capture logic into NativeFrontend, which calls IRendererBackend.
   - Result: Massive architectural shift.
3. **Split Headers:** 
   - Isolate CapturedTextureEntry, BufferEntry, TextureEntry definitions (minus D3D12 ComPtr fields, perhaps using opaque types or templates) into 
ative_frontend_types.h. 
   - Leave Renderer D3D12 specific, but allow Android to compile the frontend types.

Given the goal "Separar estado de captura/replay dos tipos COM/D3D12", Option 3 (extracting capture state) or Option 1 (Pimpl) are the most viable. Let's inspect CapturedTextureEntry and Pm4Mirror usage.

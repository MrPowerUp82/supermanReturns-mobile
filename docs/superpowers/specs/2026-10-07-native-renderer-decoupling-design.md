# Decouple Native Renderer from D3D12 Types

## 1. Context and Purpose
The PC port's 
ative_renderer.h defines class Renderer which holds both D3D12 COM types (ID3D12Device, ID3D12Resource) and frontend capture state (Pm4Mirror, texture hashes, write sequences). To enable the Android port to utilize the capture/replay logic without pulling in D3D12 dependencies (which break the build), we must separate these responsibilities.

## 2. Architecture
We will extract the frontend capture state from Renderer into a new class NativeFrontend (in 
ative_frontend.h). 
NativeFrontend will own:
- Pm4Mirror mirror_ and capture_mirror_
- Write watches, texture hashes, and CapturedTextureEntry
- Ring constants, draw call counting, and capture sequences

Renderer (in 
ative_renderer.h) will:
- Include 
ative_frontend.h
- Hold an instance of NativeFrontend (or accept it via dependency injection)
- Remain D3D12 specific, handling only actual D3D12 execution (Draw, Copy, Resources).

## 3. Impact
This ensures that 
ative_frontend.h can be included safely by sr_graphics_system.cpp and compiled on Android, while 
ative_renderer.h remains gated by D3D12 #ifdefs or simply excluded from the Android build.

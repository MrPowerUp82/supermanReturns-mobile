# Decouple Native Renderer from D3D12 Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Extract capture/frontend state from NativeRenderer to avoid D3D12 dependency leakage.

**Tech Stack:** C++23.

**Spec:** docs/superpowers/specs/2026-10-07-native-renderer-decoupling-design.md

## Task 1: Create 
ative_frontend.h
- [ ] Move Pm4Mirror includes, CapturedTextureEntry, and BufferPlan to 
ative_frontend.h.
- [ ] Define class NativeFrontend containing the guest-state tracking fields from Renderer.
- [ ] Migrate the capture logic (TextureWrittenSince, ArmTextureWatch) to NativeFrontend.

## Task 2: Refactor Renderer
- [ ] Include 
ative_frontend.h in 
ative_renderer.h.
- [ ] Make Renderer instantiate NativeFrontend frontend_; or inherit it.
- [ ] Update 
ative_renderer.cpp to use rontend_. for all capture state accesses.

## Task 3: Refactor SrGraphicsSystem
- [ ] Update sr_graphics_system.cpp to rely on NativeFrontend directly where possible.

# SDD ledger — plan: docs/superpowers/plans/2026-10-07-native-float-filter.md
Spec and plan approved by user. Native execution.
Pre-flight: tasks 1/2 share common HLSL and contract v2; task 1 prepares contract-only helper, task 2 supplies tested filtering functions. Tasks 1/3 share ABI words and revision=2; consistent. Tasks 2/3 share sampler word bits0-9; consistent. Task 4 consumes all three as atomic APK/library update.
Ruling: reuse current approved branch codex/resume-native-vulkan and existing restored dependencies; no new checkout, preserving unfinished integration and PC source. Cost if wrong: changes remain together for final review.
Ruling: use Python/PowerShell ledger/brief extraction in this Windows environment instead of bash skill scripts; same records, no product behavior change.
Task 1: start, BASE=43d9373d9090916b7738337153014068f6719541
Task 1: tests RED missing preparator, then production tooling ABI v1 mismatch and S22 offline shader assertion line23; GREEN five tool tests, all Python21, native shaders and bootstrap. Snapshots prepared; PC unchanged. Task 1 complete.
Task 2: start. HLSL probe RED use of undeclared srFilter2D. Gpu numerical tests added before helper implementation.
Task 2 Ruling: border reference fills absent R32 alpha with 1, not border alpha0; actual S22 readback and Vulkan border format-component replacement confirm only R is replaced. Corrected test reference; production filtering unchanged. HLSL reserved linear identifier renamed filterLinear; headless instance enables surface extension required by existing loader, without creating a surface.

Task 2: GREEN 23808 GPU numerical checks across three float32 formats including implicit LOD, all 241 shaders ready and independently verified. Task 3 RED missing texture_filter.h; descriptor probe added. Ruling: commit tasks 2/3 together after descriptor integration so the intermediate test target never leaves a missing production header.
Task 3: initial GREEN CPU sampling, descriptor cache + all23808 GPU checks. Independent review found pre-existing hardware base-map unrestricted LOD, directly violating guest base-map contract; separate CPU probe RED assertion line18 on S22. Add maxLod0 for hardware base-map and extend fragment proof to fractional point/linear mip LOD. Validation layer absent, no validation-layer PASS claimed.
Review follow-up confirms both source fixes. Fractional fragment test exposed GPU LOD quantization (not filtering arithmetic): expected ideal log2(1.25), driver query differs within mipmapPrecisionBits. Ruling: read back independent hardware LOD in a separate probe pass, assert its device precision bound versus analytical LOD, then retain 2e-6 filtering tolerance against CPU reference at that measured LOD. Vulkan Sampling spec explicitly allows mipmapPrecisionBits LOD precision: https://docs.vulkan.org/spec/latest/chapters/textures.html . No production change or tolerance relaxation.
Task 3 complete: all seven suites GREEN, 34560 GPU comparisons, 88 Vulkan tests. Review and follow-up resolved hardware base-map and expanded LOD proof. Python21/importer15 GREEN. Task4 full library241/241 and independent verification, game build/link and APK lint/signature/zipalign/ELF16KB GREEN; runtime relink copied into final APK E8CBC5... Device disconnected before install, user reconnected, adb install -r now Success. Library installation running, no guest launched between versions.

Task4 actual v2 boot passed packet34, loading visible but distorted, SIGSEGV. Restored APK/library/game after user authorization. Debug correction: line315 breakpoint caught handled watch epilogue; real DispatchUnhandledSignal breakpoint locates host null0x18 in Qualcomm LLVM compiler/vkCreateGraphicsPipelines. Private valid SPIR-V pair + shared fresh cache crashes mask variant0->1; raw persistent cache also crashes. Null cache compiles64 variants. Added exact Android driver quirk vendor5143/device07030001/driver80267062. Real GamePipelineStore regression RED139 then GREEN64 variants +64 object cache hits. Reviewer confirms production fix; validation error assertion added. Provider88 and preparation11 PASS. Source digest8586c25e2078b8113e09544802758d41b8573721208f368a0bfebee8150b488a. Game build completed; runtime relinked and copied fresh before final APK rebuild. Khronos1.4.363.0 external layer loaded in real boot, no VUID before driver crash. Restore saved global GPU debug settings after new boot.

2026-10-08: APK177fb0 restaurado/boot PID27213 passou falha LLVM anterior, loading
freeze breath, >44000draws. Camada Khronos ativa achou Input-08733 NORMAL1 uint4 vs
SFLOAT e maxMemoryAllocationCount-04101 (>4096). Force-stop; settings restauradas.
Investigação confirma GPU buffer individual e normal helper asfloat. RED unit
alocação31/5000 com limite32; RED contratoNORMAL1 2checks. Patches arenaGPU e
rawfloat32-input mais surface diagnostics preparados; validação em andamento.

2026-10-08: arena GPU GREEN5001valores/3alocações sob teto32. Revisão encontrou
cache descriptor usando owner como transiente; flag explícita corrige, filtro
34560comparações GREEN. NORMAL1 formatoUINT preserva asfloat. OrientaçãoIDENTITY
confirmada no S22, mas SUBOPTIMAL provocou1855recriações. Política restrita à
rotação WSI intencional: RED8/GREEN92testes, boot generation1. Gamma corrompido
por sampler metadata32..63: RED GPU índice32/GREEN1024componentes após escrita
final+flush. Todos revisados sem achados pendentes. APK659c0067/source607a5628,
runtime9422f563 instalados, biblioteca v2 inalterada. BootPID1565 com camada
Khronos chegou a Metropolis; movimento e câmera touch observados. Sem VUID/crash
até esse ponto. Coleta600s em andamento, demais gates funcionais pendentes.

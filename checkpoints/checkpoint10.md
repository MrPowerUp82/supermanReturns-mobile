# Checkpoint10 — a espera é a GPU sob throttle térmico

Continua checkpoint7/8/9. Evidência: docs/evidence/s22-maps-query-2026-10-08.md (4 etapas).

## Conclusão desta etapa

- Os spinners (sub_828A7978 = SwitchToThread) são workers ociosos do jogo, não o gargalo.
- Quadro ~322 ms = gravação ~115 + publicação ~180 no recorder. Publicação = PaintAndPresent
  inline na thread do recorder (PaintMode::kGuestOutputThreadImmediately), esperando a GPU.
- GPU: 99% ocupada, devfreq max_freq 220 MHz de 818 MHz (thermal_pwrlevel 11), Thermal 3-4,
  SoC ~60 °C, USB carregando. O jogo está limitado pela GPU em ~27% do clock.
- CPU já não é o gargalo: maps no handler, QueryRegionInfo e (opcional) yield backoff.

## Estado

- Código/ferramentas novos (commitados): runtime-patches/yield-backoff.patch (opt-in
  SR_YIELD_BACKOFF_US, padrão desligado), native-patches/zzzzzzzzzzzzzzzzz-worker-timing.patch
  (diagnóstico sob SR_VULKAN_PROFILE), native/game/main.cpp lê files/sr-debug-env.txt.
- Testes (tools/test_native.ps1 -Suite runtime -Device RXCWB05KQMX): proc_maps, region_query,
  write_watch, yield_backoff, yield_spin. 9 patches de runtime aplicam em cadeia e reversos.
- INSTALADO: APK com instrumentação (artifacts/…native-vulkan…apk). Jogo parado; env de depuração
  removido do aparelho; settings GPU globais intactas. Aparelho terminou em Thermal 4: esperar esfriar.
- Amostrador de GPU: .tools/gpu-sampler.sh; rota: .tools/run-route.sh <dir> <data>.

## Próximos passos

1. Reduzir o trabalho de GPU por quadro sem mudar resolução guest, draws nem efeitos: não há
   perfilador de GPU; opções: timestamps Vulkan por passe/pipeline, contagem de render passes e
   load/store de attachments (GPU em tiles: cada divisão custa banda), custo do filtro float32
   (taps manuais), composição final 2340x1080 com LUT de gamma.
2. Medir sempre com GPU/aparelho em estado térmico controlado: registrar devfreq/max_freq, não só
   Thermal Status; esperar Thermal 0 e preferir sem USB carregando (calor).
3. Decidir sobre o yield backoff: só vale pela CPU/calor (-55% CPU); áudio teve mais silence_chunks
   sob throttle. Reavaliar com o aparelho frio.
4. Pendências antigas: PSS ~3.5 GB, combate, qualidade de áudio, coletor 600 s, teste Python
   test_production_patches_apply_to_pc_checkout (PC avançou).

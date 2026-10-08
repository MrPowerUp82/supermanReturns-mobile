# Checkpoint9 — handler sem maps; gargalo agora é sincronização

Continua checkpoint7/8 (build, clone fixado .tools/pc-ae51078, run-as simpleperf, adb install -r,
.tools/run-route.sh). Evidência: docs/evidence/s22-maps-query-2026-10-08.md (3 etapas).

## Estado em 2026-10-08

- Falha de write-watch sem releitura do maps: tools/runtime-patches/mmio-fault-fast-path.patch
  (callback primeiro; consulta ao host só se recusar; recuperação confere se a página
  já está gravável para não gerar WARN na corrida benigna).
- Teste: tests/native/test_write_watch.cpp na suíte `tools/test_native.ps1 -Suite runtime`
  (-Device RXCWB05KQMX; roda test_proc_maps, test_region_query, test_write_watch).
  868.7 µs -> 2.7 µs por falha com 6000 mapeamentos; corrida de 4 threads passa.
- Jogo: ExceptionCallback 56% -> 1.8% da thread de render; render 93% -> 37% ocupada;
  frames 1494 vs 1201 em 409 s, mas ~2.5 f/s no trecho final (igual). Ainda não jogável.
- INSTALADO: APK 7640d813… (artifacts/superman-returns-native-vulkan-0.1.0-dev.apk), 22
  patches (runtime: bounded-region-query, mmio-fault-fast-path, proc-maps-fast-query; renderer: 20).
  Dados e biblioteca v2 intactos. Jogo parado; settings GPU globais intactas.
- Dados privados: .tools/mmio-fix (relatórios, render-inclusive.txt, game.log, mmiofix.data).

## Próximos passos

1. Descobrir o que as threads que giram esperam. 4 threads em sub_828A7978 ->
   NtYieldExecution -> sched_yield (49-97%) enquanto render (37%), submissão (30%) e áudio
   (54%) não saturam. Tentar `simpleperf record --trace-offcpu` (pode exigir tracepoints) ou
   instrumentar/ler o que sub_828A7978 testa (flag/fence/vblank). Hipóteses: espera por
   fence/swap da submissão Vulkan (record_submit_ms ~117 ms, ~3100 draws/quadro), ou
   pacing de vblank do guest.
2. Custos por quadro que sobram: record_submit_ms (~117 ms), CaptureTextures (~18% da thread
   de render), ~3100 draws por quadro; NativeFrontend::DrawIndexedVertices 31%.
3. Esperar Thermal 0 antes de medir (todas as rodadas terminaram em Thermal 3) e fixar a cena.
4. Pendências: PSS 3.56 GB e crescendo com o progresso (investigar retenção), combate,
   qualidade de áudio, coletor 600 s, teste Python test_production_patches_apply_to_pc_checkout.

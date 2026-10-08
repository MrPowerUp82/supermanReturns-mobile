# Checkpoint8 — QueryRegionInfo limitado / gargalo agora é o maps no handler

Continua o checkpoint7 (cuidados de build, clone fixado .tools/pc-ae51078, run-as
simpleperf, adb install -r). Detalhes e tabela: docs/evidence/s22-maps-query-2026-10-08.md.

## Estado em 2026-10-08

- QueryRegionInfo (46% da thread de render) agora 2.9%: patches
  tools/runtime-patches/bounded-region-query.patch (SDK, parâmetro max_region_size
  opcional) e tools/native-patches/zzzzzzzzzzzzzzzz-bounded-region-scan.patch
  (ValidatedGuestPointer passa end-page, limitado a UINT32_MAX).
- Testes no S22 (tools/test_native.ps1 -Device RXCWB05KQMX): -Suite runtime
  (test_proc_maps + test_region_query), frontend e commands passam.
- Vazão (logs do jogo): ~1.2 f/s -> 1.9-3.2 -> 2.6-3.5 f/s. Ainda ~3 f/s, não jogável.
  Cada rodada terminou em Thermal 3; cenas não idênticas: só ordem de grandeza.
- INSTALADO: APK 25299e8f… (artifacts/superman-returns-native-vulkan-0.1.0-dev.apk),
  source ae5107814a56, 20 patches de renderer, shader 36a62692…. Instalado com -r,
  dados e biblioteca v2 intactos. Jogo parado; settings GPU globais intactas.
- Script privado da rota fria + perfil: .tools/run-route.sh <dir> <nome.data>
  (launcher -> título -> START -> A -> 340 s -> simpleperf 15 s). Dados em .tools/region-fix.

## Próximos passos

1. Gargalo atual: ExceptionCallback 56% da thread de render, 44% só no read do
   /proc/self/maps (kernel formatando VMAs) a cada falha de write-watch. Medir falhas
   por segundo e por faixa de endereço; opções: evitar QueryProtect no handler (o
   recheck só descarta a corrida em que outra thread já limpou o watch), ou reduzir o
   número de falhas. Mexer no handler é semântico: precisa teste da corrida.
2. Depois: CaptureTextures (~8%) e DrawIndexedVertices; sched_yield nas 3 threads
   saturadas (esperas ativas) consome ~44% das amostras totais da CPU.
3. Esperar o aparelho em Thermal 0 antes de medir e fixar a cena para comparar vazão.
4. Pendências antigas: crescimento de memória (PSS 3.3 GB), combate, qualidade de áudio,
   coletor de 600 s, teste Python test_production_patches_apply_to_pc_checkout (PC avançou).

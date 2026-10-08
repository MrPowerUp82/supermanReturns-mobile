# Checkpoint7 — QueryProtect rápido / próximos hotspots

Workspace: C:/Users/webpa/OneDrive/Documentos/projetos/supermanReturns-mobile (outro PC que o checkpoint6).
ADB RXCWB05KQMX, Galaxy S22 SM-S901E / Android16 / Adreno730. Branch main.
Detalhes e números: docs/evidence/s22-maps-query-2026-10-08.md.

## Estado em 2026-10-08

- Perfil sem camada confirmou o hotspot: FGGameRender 62.3% em ExceptionCallback →
  QueryProtect → releitura de /proc/self/maps (ifstream+sscanf).
- Correção em tools/runtime-patches/proc-maps-fast-query.patch (header puro
  src/core/proc_maps.h + open/read). Teste: tools/test_native.ps1 -Suite runtime
  -Device RXCWB05KQMX (NDK, roda no S22). RED e GREEN observados.
- Resultado: ExceptionCallback 62.3% -> 27.3% na thread de render; frames guest
  ~1.2 f/s -> 1.9-3.2 f/s (cenas não idênticas; ganho 1.6-2.7x). Ainda não jogável.
- INSTALADO: APK 9f8348bc… (artifacts/superman-returns-native-vulkan-0.1.0-dev.apk,
  source ae5107814a56, digest 4bc9f178…, shader 36a62692…). Jogo parado, settings GPU
  globais intactas (enable0, demais ausentes). Pacote .test preservado.
- Pacote .native foi desinstalado e reinstalado: chave de debug deste PC difere da do
  outro. Dados restaurados e conferidos por hash (biblioteca v2 + 13/13 arquivos do
  jogo). Backup privado: .tools/appdata-backup-20261008/appdata.tar. Staging do jogo
  em /data/local/tmp/sr-native-20261007-1928/game preservado. O APK 659c não existe mais.
- Próxima instalação neste PC usa adb install -r (mesma chave); não precisa desinstalar.

## Cuidados descobertos

- O checkout ../superman_returns_recomp avançou (HEAD 4400485 + 10 arquivos sujos) e
  já contém android_provider.cpp/shader_process_android.h. Não construir a partir dele:
  usar o clone fixado privado .tools/pc-ae51078 (ae5107814a56, autocrlf=false):
  python -B tools/prepare_native_renderer.py --recomp .tools/pc-ae51078 (+ --verify).
- tests/test_prepare_native_renderer.py::test_production_patches_apply_to_pc_checkout
  falha por isso (patches criam arquivos que o PC já tem). Preexistente, não é da
  correção. Decidir: fixar o commit do PC no teste ou rebasear os patches.
- Build: cmake configure + cmake --build .tools/game-build --target superman_game
  (reconfigura sozinho), copiar libsuperman_game.so e librexruntime.so para
  android/app/libs/arm64-v8a, depois tools/build.ps1 -NativeSideBySide.
- Digest muda por CRLF/LF (source-lock), não por código; caches de driver ficam frios.
- Perfil: adb shell run-as <pkg> /system/bin/simpleperf record -p PID -e cpu-clock
  -f 99 -g --duration 15 -o files/x.data (como shell o evento é rejeitado). Reinstalar
  apaga files/*.data; manter cópia no PC.
- Git Bash: usar MSYS_NO_PATHCONV=1 e MSYS2_ARG_CONV_EXCL="*" com caminhos /data/....

## Próximos passos

1. Atacar QueryRegionInfo (46% da thread de render): chamado por ValidatedGuestPointer
   → NativeFrontend::PlanStreams/DrawVertices; varre a região inteira página a página.
   Verificar se basta limitar a varredura ao comprimento pedido (SDK xmemory.cpp ou
   patch em tools/native-patches). Medir antes/depois com a mesma rota.
2. Reduzir o custo restante de falhas de write-watch (read 21% = kernel formatando
   maps): contar falhas por segundo; evitar a consulta no handler ou reduzir falhas.
3. Comparação de vazão com cena idêntica e aparelho em Thermal 0 (esperar esfriar).
4. Investigar crescimento de memória (PSS 3.09-3.24 GB; sem dado novo de retenção),
   completar combate e qualidade de áudio (guest chegou a 8 frames/s de áudio contra
   187/s em cinemática, com silence_chunks altos), repetir coletor corrigido de 600 s.
5. Sem PASS funcional até esses gates.

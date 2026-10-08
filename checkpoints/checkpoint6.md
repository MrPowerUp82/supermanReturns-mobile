# Checkpoint6 — renderer Vulkan nativo / filtro float32 v2

Workspace: C:/Users/Gusta/Documents/outros-projetos/supermanReturns-mobile.
PC read-only: ../superman_returns_recomp, commit ae5107814a56. Branch codex/resume-native-vulkan; correções do renderer commitadas em bb5ce69.
Usuário autorizou continuar e restaurar a build nativa. Não pedir novamente.

## Estado atual em 2026-10-08

Dados do jogo e biblioteca v2 restaurados no pacote org.supermanreturns.mobile.native.
ADB RXCWB05KQMX, Galaxy S22 SM-S901E / Android16 / Adreno730.
Preservar pacote .test e checkout PC, ASTs e layout guest1280x720.

Filtro float32/ABI v2 em c082611: 34560 comparações GPU. Biblioteca privada
SHA36a62692... (hash completo nas evidências), 241 shaders, nenhum omitido.

Correções locais verificadas:
- Cache VkPipelineCache causa crash LLVM no driver Adreno exato 0x80267062.
  Quirk usa VK_NULL_HANDLE e preserva cache de objetos. RED139, GREEN64variantes+64hits.
- Buffers individuais excedem4096alocações. Arena GPU8MB:5000IDs unit e5001valores
  lidos da GPU em3alocações sob teto32. Flag transient explícita preserva cache de descriptors.
- NORMAL1 uint4 usa asfloat: atributos float32 precisam formato UINT de mesma largura.
- Surface2340x1080 currentTransformROTATE90/supported511, compositor não prerotaciona.
  EscolherIDENTITY corrigiu orientação, mas exigiu tratar SUBOPTIMAL da apresentação.
- Política allow_suboptimal somente quando preTransform difere de currentTransform.
  RED8presents, GREEN92testes. OUT_OF_DATE/device lost/resize preservados.
- Metadados sampler shared+128 corrompiam gamma[32..63] do compositor.
  Escrever LUT/options após Prepare e FlushTransient novamente. RED GPU no índice32,
  GREEN256entradas/1024componentes com compositor real e LUT assimétrica.

## Execução e artefatos

BootAPK08ecf3... (source53a87...): PID29550, camada Khronos ativa,
417501draws/frame359, live_buffers51, sem VUID/crash observado. Chegou ao diálogo
Start New Game/Load Game. Não é gameplay. Parado para atualização.

BootAPK530fa...: PID31317, imagem upright, mas generation1855 de swapchain;
parado, settings restaurados. Não usar como build estável.

ATUALMENTE INSTALADO: APK659c0067... com política SUBOPTIMAL e correção gamma.
PIDgame1565 foi encerrado após a coleta, log .tools/composition-boot.log.
Camada Khronos estava ativa durante o teste; settings globais restaurados:
enable_gpu_debug_layers0, gpu_debug_app/layers/layer_app ausentes.
Log confirmou present=1000001003 (SUBOPTIMAL) e intentional_wsi_rotation1,
swapchain generation1. No encerramento do teste, restaurar settings conforme
.tools/filter-validation-settings-backup.json: enable0 e demais chaves ausentes.

APK COM GAMMA instalado com sucesso. Logs
.tools/composition-{gamma-green,game-build,apk}.log. Teste composition passou.
Loading Superbreath e título upright, sem posterização anterior. Novo jogo chegou
a Metropolis; movimento e câmera touch responderam, com cores plausíveis comparadas
qualitativamente à referência PC. Coleta600s ativa sessão4449, diretório
.tools/native-float-validation terminou600.81s. Voo e pouso por Y observados. Pause Menu viaSTART,
HOME/retorno preservaram PID1565, A retomou. Generation2 corresponde ao retorno
da surface. Ainda faltam combate e qualidade áudio. Captura áudio output18.82s
não silenciosa (média-39.8dB/pico-25.4dB); captura playback durante pausa silenciosa.
Revisão agregada sem regressão concreta; resumo de evidências atualizado a pedido.
Coletor rejeitou linhas epoch com9espaços iniciais; teste RED/GREEN6tests,
regex corrigida e revisada. summary.json original preservado; counter-readback.json
separado confirma360->720,930529draws adicionais,0.748swapsguest/s (não apresentação).
PSS3050233->3722753KB, pico3734355KB. Nenhum VUID ou fatal signal no intervalo.
Não marcar600s como aceitação final: crescimento de memória e desempenho pendentes.
Perfil15s/.tools/composition-perf.{data,txt}:4456amostras,0perdidas; hotspots
FGGameRender QueryRegionInfo, parsing de memória, guest/audio e sched_yield.
Thermal Status3 observado. Próxima execução após resfriar, sem camada, para
comparação de desempenho e investigação de retenção; depois combate/áudio.
Copiar runtime fresco após relink antes de assemble, como no comando atual.
Hashes completos e histórico: docs/evidence/s22-native-float-filter-2026-10-07.md.

## Próximos passos

1. Reabrir APK659c após resfriar e coletar perfil sem camada, preservando ASTs,
   resolução guest, draws/efeitos e biblioteca v2. Confirmar hotspots antes de corrigir.
2. Investigar crescimento de memória e comparar mesma rota fria/aquecida.
3. Completar combate e qualidade áudio; comparar mesmo enquadramento PC e repetir
   coletor corrigido600s. Sem PASS funcional até esses gates.
4. Atualizar evidências/checkpoint e commitar somente código/docs/testes/patches;
   excluir .superpowers, APKs, biblioteca, logs/dumps e dados comerciais.

Snapshot privado .tools/pc-native recebe19patches versionados de tools/native-patches.
Não editar PC. Repreparar preservando mtime apenas para arquivos byte-idênticos
(evita recompilar todo núcleo). --verify antes do build. Ferramentas de shaders
privadas .tools/native-shader-tools: usar Python -B, ABI v2 mantém biblioteca atual.

Revisor review_float_filter já revisou cache, arena, formato, orientação,
SUBOPTIMAL e gamma; sem achados pendentes. Achado sobre cache de descriptors corrigido.
Builds/testes: tools/test_native.ps1 suítes provider/filter/pipeline/allocations/composition.
Camada ausente no headless; ativa nos boots pelo loader Android externo.

Launcher FQN org.supermanreturns.mobile.LauncherActivity. GameActivity não exportada.
Após instalar: abrir launcher, aguardar montagem, scroll, uiautomator dump e botão
Iniciar jogo experimental. Coordenadas observadas portrait[78,1392][1002,1554].
Coletar logcat do PID :game, não launcher. Touch START no landscape≈1312,320;
manter1segundo para o guest registrar, depois A≈2008,778. Usar screenshot atual.

PC referência já inspecionada (cidade correta):
../superman_returns_recomp/logs/bench_native_c3_final_after_vulkan_idle.png.
Staging do jogo preservado em /data/local/tmp/sr-native-20261007-1928/game.
Não limpar antes de conferir todos os arquivos.

# Migração para Android

Estado atual: o alvo `native/game` usa o core Vulkan nativo do PC. A extensão
float32 e o contrato v2 passaram nas suítes do S22 e estão no APK compilado.
O crash de compilação no driver Qualcomm foi isolado e contornado para o
driver exato do S22. Arena GPU, NORMAL1, orientação, política SUBOPTIMAL e LUT
gamma corrigidos e testados. APK659c0067 chegou a Metropolis; movimento, câmera,
voo, pouso, pausa e retorno do app observados. Captura600s sem crash observado;
desempenho baixo e crescimento de memória ainda precisam investigação.
Combate e qualidade do áudio continuam pendentes. Ver
[relatório atual](evidence/s22-native-float-filter-2026-10-07.md).
As descrições posteriores do placeholder e da factory Xenos são históricas.

Alvo informado pelo usuário: Galaxy S22 Snapdragon. Confirmar a versão do
Android durante teste real; o APK exige Android 13 ou superior. Em 2026-10-07,
o aparelho SM-S901E foi conectado por ADB: Android 16, SoC SM8450, Adreno 730.
O diagnóstico apresentou quadros Vulkan e passou pelos testes de touch,
rotação e reabertura. Isso ainda não valida gameplay.

## Arquitetura atual

```text
LauncherActivity ─ InstallController ─ XboxIso ─ InstallStore
                      │                            │
                      └ worker único               └ armazenamento privado

DiagnosticsActivity ─ ControllerView ─ NativeBridge (JNI)
       │                                    │
       └ HandlerThread ─ Surface Android ─ núcleo Vulkan do port PC

superman_returns_recomp/port/generated/default
       └ CMake independente ─ libsuperman_guest_codegen.a (somente compilação)
```

O fluxo de diagnóstico não carrega o archive do jogo. A build opcional de
gameplay adiciona `libsuperman_game.so` e `librexruntime.so` (SDL linkado no
runtime), uma Activity separada e o botão **Iniciar jogo experimental**.
`tools/build_game.ps1` recompila os sources gerados com os mesmos headers do
fork Android antes de linkar; o archive de verificação v0.10 não é reutilizado.

## Por que não transplantar o Skate3-Mobile inteiro

Seus hooks, shaders, Title Update, EAWebKit, mods e endereços de memória são
específicos do Skate 3. Superman usa somente `default.xex` e `DATA/*.AST`.
O projeto local gera código ReXGlue v0.10.0; o fork Android de referência possui
uma base SDK diferente. Compatibilidade de ABI e comportamento ainda não foi
verificada em toda a gameplay. A integração remove apenas os metadados
`codegen_flags` com todos os campos falsos, recompila com os headers do fork
e preserva o hook XMA. O boot real foi observado até os logos e o loading.

O primeiro teste encontrou uma corrida na atribuição do nome das threads,
corrigida pelo patch versionado `tools/runtime-patches/thread-name-race.patch`.
O segundo teste foi encerrado pelo Android por LOW_MEMORY durante o loading.
O perfil atual usa escala 1× e cache de texturas soft/hard de 256/512 MB,
em vez dos defaults 2× e 2048/4096 MB herdados do fork.

## Componentes do port PC a separar

| Componente PC | Ação necessária |
| --- | --- |
| `port/CMakeLists.txt` | Retirar o gate `WIN32` do build do renderer; manter D3D12 fora do alvo Android |
| `native_renderer/native_renderer.h` | Separar estado de captura/replay dos tipos COM/D3D12 |
| `native_renderer/native_graphics_system.*` | Implementar contrato GPU guest usando o host Vulkan Android |
| `graphics/vulkan/platform/native_provider.cpp` | Substituir provider e integração UI do desktop |
| `graphics/vulkan/platform/shader_process.cpp` | Substituir `CreateProcessW` por tradução local ou shaders preparados pelo usuário |
| `native_renderer/xxh3_avx2.cpp` | Usar caminho escalar/ARM, sem flags AVX/SSE no Android |
| `src/xma_fixes.cpp` | Revalidar o hook no runtime Android com a mesma imagem do jogo |
| `superman_returns_app.h` | Configurar paths privados e factory Vulkan, sem fallback para plugin D3D12 |

O núcleo copiado em `native/vulkan/` já tem loader POSIX/Android. Esta etapa
usa apenas Context, Swapchain e FrameLoop; recursos, texturas, draw packets,
pipeline e composição de gameplay ainda ficam no projeto PC.

## Gates antes de chamar uma build de jogável

- Link completo de CPU guest + runtime + hooks sem símbolos indefinidos.
- Boot e leitura dos AST em filesystem Android sensível a maiúsculas.
- Menus, intro e início da gameplay com imagem comparável ao PC.
- Movimento, câmera, voo e combate usando touch e controle físico.
- Áudio contínuo sem bloquear o mixer ou corromper o heap.
- Pausar/retomar, rotacionar, voltar ao launcher e abrir nova sessão.
- Cache de shaders persistente, sem executáveis Windows ou dados de outro jogo.
- Medições de frame time, RAM e aquecimento depois de estabilizar a cena.

O link completo, o carregamento do XEX, logos, apresentação de frames, dados
de áudio não silenciosos e entrada XInput touch foram observados no aparelho.
Esses resultados ainda não aprovam todos os gates de gameplay acima.

## Continuação do checkpoint 2

A factory Android usa explicitamente `rex::graphics::vulkan::VulkanGraphicsSystem`.
A classe `VulkanNativeGraphicsSystem` do port PC ainda contém stubs: não cria
presenter nem processador de comandos e não herda o contrato concreto usado
por `ReXApp` para acessar apresentação e armazenamento de shaders. Ignorar a
inicialização de shaders evitaria apenas um acesso inválido, sem implementar GPU.
O alvo Android deixou de compilar esse placeholder; o trabalho de separação
do renderer PC permanece disponível no projeto original.

No teste no S22, a factory completa passou por `InitializeShaderStorage`,
iniciou a thread guest com status `00000000`, apresentou loading e menu de
pausa e respondeu ao touch. A cena 3D ficou preta. O PSS observado chegou a
aproximadamente 3,3 GB, dos quais 2,7 GB classificados como Graphics. Isso
continua sendo uma build experimental, sem validação de gameplay.

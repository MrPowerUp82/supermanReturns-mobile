# Migração para Android

Alvo informado pelo usuário: Galaxy S22 Snapdragon. Confirmar a versão do
Android durante teste real; o APK exige Android 13 ou superior. Não há aparelho
conectado por ADB nesta sessão, então ainda não há evidência de execução nele.

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

O fluxo de diagnóstico não carrega o archive do jogo. Não existe botão Jogar
que prometa gameplay ainda indisponível. O APK não carrega `rexruntime`, SDL,
drivers customizados ou bibliotecas do Skate3-Mobile.

## Por que não transplantar o Skate3-Mobile inteiro

Seus hooks, shaders, Title Update, EAWebKit, mods e endereços de memória são
específicos do Skate 3. Superman usa somente `default.xex` e `DATA/*.AST`.
O projeto local gera código ReXGlue v0.10.0; o fork Android de referência possui
uma base SDK diferente. Compatibilidade de ABI e comportamento ainda não foi
verificada. Não misturar os runtimes apenas por ambos suportarem ARM64.

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

Os testes atuais verificam importação e compilação. Nenhum desses gates de
gameplay foi aprovado ainda.

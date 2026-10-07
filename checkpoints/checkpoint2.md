# Checkpoint 2: Android Vulkan Factory & Crash Analysis

## Resumo do Progresso (2026-10-07)
Nesta sessão, avançamos as etapas finais do roadmap `docs/android-port.md`:
1. **Desacoplamento do Frontend D3D12**: Extraímos as estruturas de estado de captura/PM4 (como `Pm4Mirror`, `CapturedTextureEntry`) da classe `Renderer` para a classe base `NativeFrontend` no arquivo `native_frontend.h`. Isso isolou a dependência `<d3d12_api.h>` da lógica de tracking do jogo.
2. **Setup da Factory Vulkan no Android**: Adicionamos o mapeamento em `native/game/main.cpp` injetando diretamente o `VulkanNativeGraphicsSystem` no `config.graphics`, bypassando qualquer inicialização falha do Xenos D3D12. 
3. **Build e Deploy do APK**: Compilamos com sucesso o `libsuperman_game.so` (resolvendo a quebra de build gerada por uma macro `X_STATUS_SUCCESS`). Geramos o pacote e o instalamos usando o ADB no dispositivo de destino (Galaxy S22).

## O Gargalo Atual (Crash em Runtime)
Ao acionar o botão **"Iniciar jogo experimental"** na interface Android, o jogo capotou na Thread da UI durante o startup do framework SDK.
**Logcat (libsigchain):**
```
10-07 11:54:37.186 21946 21974 E libsigchain:   #03 pc 00932c64 (rex::graphics::CommandProcessor::CallInThread)
10-07 11:54:37.186 21946 21974 E libsigchain:   #04 pc 0092d6c0 (rex::graphics::GraphicsSystem::InitializeShaderStorage)
```

**Causa Raiz:**
No código do SDK base (pré-compilado em `.references/rexglue-sdk/src/ui/rex_app.cpp:802`), há o seguinte trecho:
```cpp
auto* graphics_system = static_cast<rex::graphics::GraphicsSystem*>(runtime_->graphics_system());
if (graphics_system && !runtime_->cache_root().empty()) {
  graphics_system->InitializeShaderStorage(...);
}
```
A arquitetura do `rex_app.cpp` no SDK base faz um `static_cast` cego da interface pura `IGraphicsSystem*` (que o nosso `VulkanNativeGraphicsSystem` implementa) para a implementação completa `rex::graphics::GraphicsSystem*` (a qual nós **NÃO** herdamos). Como o cast funciona cegamente e o método `InitializeShaderStorage` acessa o `command_processor_`, ocorre um segmentation fault por VTable mismatch ou desreferenciamento de ponteiro corrompido (`CallInThread` crasha).

No projeto PC, isso possivelmente não quebra porque o build PC não depende da interface visual padrão ou porque o `NativeGraphicsSystem` da versão PC estende a classe certa com os stubs, algo que deve ser espelhado.

## Próximos Passos para o Codex
1. Analisar como o `StrictNativeGraphicsSystem` ou o runtime no PC (`sr_graphics_system.cpp`) evitam esse cast perigoso ou como implementar um _dummy/mock_ do `rex::graphics::GraphicsSystem` que possa herdar as dependências de base sem crashar ao inicializar o armazenamento de shaders.
2. Considerar a criação de um wrapper (`AndroidGraphicsSystemAdapter`) herdando de `rex::graphics::GraphicsSystem` (se suas dependências D3D12 permitirem via includes `#ifdef`), de modo que o `rex_app.cpp` não trave. Alternativamente, verificar se existe um _patch de runtime_ (`tools/runtime-patches/`) que possa anular essa chamada.
3. Após resolver o crash de runtime, revalidar o `src/xma_fixes.cpp` acompanhando os logs do áudio na gameplay.

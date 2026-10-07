# PC Native Vulkan Android Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Executar a área inicial de Superman Returns no S22 com o renderer nativo Vulkan do PC, incluindo cidade, movimento, voo, combate, áudio e controles.

**Architecture:** Preparar uma cópia privada e rastreável das fontes PC e aplicar adaptações versionadas. Separar captura de D3D12, corrigir o contrato gráfico do SDK, conectar o sistema de comandos nativo ao core Vulkan PC e fornecer apresentação Android e biblioteca de shaders local. Nenhum draw nativo será delegado ao backend Xenos do SDK.

**Tech Stack:** C++23, Vulkan, Android ARM64, NDK 27.2.12479018, SDK 35, JDK 17, CMake/Ninja, Python e PowerShell; ferramentas de shader executadas no Windows durante a preparação.

**Spec:** `docs/superpowers/specs/2026-10-07-pc-native-vulkan-android-port-design.md` — aprovada pelo usuário em 2026-10-07. Este plano aguarda revisão e escolha de execução.

## Global Constraints

- Destino: SM-S901E, Snapdragon 8 Gen 1, Adreno 730, Android 16, ARM64.
- Reutilizar o núcleo nativo Vulkan e a lógica de captura do PC.
- O worker Vulkan recebe snapshots possuídos pelo pacote e não lê memória guest mutável.
- Não sobrescrever alterações preexistentes do usuário.
- Manter inicialmente o layout interno 1280x720 do PC.
- Não existe fallback automático para o renderer Xenos.
- A biblioteca contém dados derivados do jogo: não será commitada nem incorporada ao APK público.
- Não modificar ou apagar ASTs para contornar áudio.
- 30 FPS é a meta de desempenho, não um resultado garantido por esta integração.
- Executar uma sessão de pelo menos 10 minutos na área inicial, incluindo pausa e retomada após ir ao fundo.

## Review Focus

1. Fonte PC com alterações locais: preparar uma cópia rastreável sem perder mudanças nem compilar arquivos antigos deixados de uma preparação anterior (tarefa 1).
2. Ponteiro guest inválido, alias físico ou overflow: recusar leitura sem acessar memória host inválida e sem mudar dados capturados depois da publicação (tarefa 3).
3. Biblioteca ausente, truncada ou shader criado em runtime não coberto: impedir boot inválido ou interromper com container/estágio/hash recuperáveis, sem omitir draws silenciosamente (tarefa 4).
4. Superfície destruída com submissão pendente: aguardar a posse por fence, desligar a apresentação e recriar a janela sem usar handles antigos (tarefa 6).
5. Fechar ou pausar durante espera de progresso e input pressionado: acordar workers, soltar botões e concluir shutdown sem deadlock (tarefas 5 e 7).

## Convenções, fontes e mapa de arquivos

Neste plano, `PC` significa `C:/Users/webpa/OneDrive/Documentos/projetos/superman_returns_recomp`.
Os caminhos abaixo são relativos ao projeto mobile, salvo os prefixados por `PC/`.
Arquivos PC são referências de leitura: as adaptações entram em patches mobile,
aplicados à cópia privada `.tools/pc-native`. Não editar o checkout PC durante a
execução deste plano. Essa decisão preserva o trabalho preexistente naquela pasta.

- `tools/prepare_native_renderer.py`, `tools/native-source-manifest.json`: seleção e verificação de fontes; snapshot privado e hashes.
- `tools/native-patches/*.patch`: adaptações do frontend, comandos, provider, shader service e CMake da cópia PC.
- `tools/runtime-patches/native-graphics-contract.patch`: contrato de injeção gráfica do SDK Android.
- `native/game/native_bootstrap.h/.cpp`: configuração de caminhos, factory nativa e pré-requisitos Android.
- `native/game/CMakeLists.txt`, `native/game/main.cpp`, `native/game/hooks.cpp`: conexão do renderer completo à biblioteca do jogo.
- `native/renderer-tests/CMakeLists.txt`, `tests/native/*.cpp`, `tools/test_native.ps1`: testes de componentes, contratos e integração ARM64.
- `tools/prepare_native_shaders.ps1`: geração, validação e instalação privada da `.srvk`.
- `tools/validate_native_device.ps1`: coleta restrita ao processo do jogo, screenshots, memória e desempenho.
- `docs/validation.md`, `docs/android-port.md`, `THIRD_PARTY_NOTICES.md`: evidências, instruções e proveniência.

Arquivos novos dentro da árvore PC privada são entregues por patches que incluem
suas criações. A preparação registra a revisão, hashes originais e hashes finais.
Scripts PowerShell devem propagar exit codes; usar Python pelo interpretador já
disponível na máquina e JDK/SDK locais. Não instalar dependências antes de constatar
que as ferramentas existentes são insuficientes.

## Ordem e verificação

Dependências: `1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8`. Cada tarefa termina com
uma unidade verificável, embora somente a tarefa 7 conecte o novo caminho ao APK.
Não substituir a factory antes disso. A tarefa 8 corrige defeitos encontrados no
roteiro dentro das fronteiras implementadas; uma captura sem cenário não encerra
a tarefa. Se faltar suporte substantivo do core, registrar a causa e revisar o
plano antes de introduzir outro renderer.

`tools/test_native.ps1 -Suite <nome>` terá nomes `source`, `contract`, `frontend`,
`shaders`, `commands`, `provider` e `bootstrap`. A suíte `source` usa unittest;
as demais usam CTest com testes C++ dos componentes. Testes dependentes do SDK
Android são compilados com NDK e executados via ADB quando `-Device RXCWB05KQMX`
é passado; os testes portáveis rodam no host. Falha de conexão ou ausência de
executável é FAIL, nunca teste aprovado. O script imprime casos executados e
retorna código diferente de zero em qualquer falha. A seleção de suíte e testes
fica no CMake, sem reimplementar a lógica de produção nos scripts.

### Tarefa 1: Preparação rastreável e reproduzível do núcleo PC

**Files:** criar `tools/prepare_native_renderer.py`, `tools/native-source-manifest.json`, `tests/test_prepare_native_renderer.py`, `tools/test_native.ps1`; modificar `tools/build_game.ps1`, `tools/collect_runtime_notices.py`, `THIRD_PARTY_NOTICES.md`.

**Interfaces:** `prepare_native_renderer.py --recomp <PC> --output <dir> [--verify]` produz `source-lock.json` com commit PC, hash SHA-256 por arquivo de entrada, alterações locais relevantes, patches aplicados e hashes finais. `--verify` confere o snapshot existente sem modificá-lo. `test_native.ps1 -Suite source` executa `python -m unittest discover -s tests -p test_prepare_native_renderer.py`.

- [ ] Escrever testes `test_dirty_source_is_recorded`, `test_removed_source_does_not_survive`, `test_verify_detects_tamper` e `test_incompatible_patch_keeps_previous_snapshot`: conferir conteúdo copiado, hashes, ausência de arquivo removido, exit code não zero e preservação do último snapshot válido.
- [ ] Executar a suíte `source`; confirmar falha pela ausência da ferramenta.
- [ ] Implementar preparação por staging privado e substituição apenas após verificação; restringir todos os destinos à pasta de saída. Incluir dependências transitivas de `native_renderer`, `graphics/guest`, `graphics/shaders`, `graphics/vulkan`, seus shaders auxiliares, perfil do jogo e cabeçalhos de suporte realmente consumidos; não copiar ROMs, logs ou caches de shader.
- [ ] Integrar preparação antes do CMake em `build_game.ps1`; registrar licenças e origem dos arquivos selecionados. As correções de compatibilidade do SDK e dos fontes PC ficam em diretórios de patches distintos.
- [ ] Rodar `tools/test_native.ps1 -Suite source` e preparação seguida de `--verify`; exigir PASS e hashes estáveis sem mudança de entrada.
- [ ] Commitar somente arquivos desta tarefa: `build: prepare traceable PC native Vulkan sources`.

### Tarefa 2: Contrato gráfico real do runtime

**Files:** criar `tools/runtime-patches/native-graphics-contract.patch`, `native/renderer-tests/CMakeLists.txt`, `tests/native/test_graphics_contract.cpp`; modificar `tools/test_native.ps1`. O patch altera `include/rex/system/interfaces/graphics.h`, `include/rex/graphics/graphics_system.h`, `src/ui/rex_app.cpp` e `src/kernel/xboxkrnl/xboxkrnl_video.cpp` do SDK; incluir outros consumidores descobertos na auditoria de casts.

**Interfaces:** adicionar a `IGraphicsSystem` métodos virtuais `ui::GraphicsProvider* provider() const`, `ui::Presenter* presenter() const`, `void InitializeShaderStorage(const std::filesystem::path&, uint32_t, bool)`, `void SetInterruptCallback(uint32_t,uint32_t)`, `void InitializeRingBuffer(uint32_t,uint32_t)`, `void EnableReadPointerWriteBack(uint32_t,uint32_t)` e `void SetSystemCommandBufferGpuIdentifierAddress(uint32_t)`. `GraphicsSystem` implementa com o comportamento existente. O nativo implementará na tarefa 5. Ajustar o `INativeGraphicsSystem` privado para não duplicar contratos com assinaturas diferentes.

- [ ] Criar `routes_provider_and_shader_storage_without_xenos`, `routes_video_ring_interrupt_and_identifier` e `shutdown_without_presentation`: usar um fake que deriva exclusivamente de `IGraphicsSystem`; assertar ponteiros, argumentos, ordem e contagem exata de callbacks recebidos.
- [ ] Rodar `tools/test_native.ps1 -Suite contract -Device RXCWB05KQMX`; esperar erro de compilação por contrato ausente.
- [ ] Implementar os métodos e substituir os casts nos consumidores pelos acessos de interface. Migrar os demais consumidores concretos de pausa/retomada/cache/tracing identificados: operações requeridas pelo ciclo de vida tornam-se virtuais; operações opcionais usam detecção explícita de suporte e mensagem de indisponibilidade. Não criar um `CommandProcessor` Xenos para satisfazer uma chamada.
- [ ] Garantir inicialização da apresentação antes do guest e armazenamento de shaders antes de Resume; falha impede lançamento. Atualizar os testes para incluir a ordem real do startup e as operações adicionais encontradas.
- [ ] Rodar a suíte `contract` e `tools/build_game.ps1`; exigir PASS dos testes e build do APK atual com o backend existente ainda selecionado.
- [ ] Commitar patch, testes e runner: `fix: support native graphics contracts in Android runtime`.

### Tarefa 3: Captura nativa independente de D3D12

**Files:** criar `tools/native-patches/frontend-portable.patch`, `tests/native/test_frontend_packets.cpp`, `tests/native/test_guest_reads.cpp`; modificar runner e CMake dos testes. Referências: `PC/port/src/native_renderer/native_frontend.h`, `native_renderer.h/.cpp`, `checked_guest_memory.h`, `native_hooks.cpp`, `pm4_mirror.*`, `PC/port/src/graphics/guest/*`.

**Interfaces:** na cópia privada, mover as operações de captura usadas pelos hooks para `NativeFrontend`, preservando nomes e assinaturas existentes (`DrawVertices`, `DrawIndexedVertices`, `DrawInlineVertices`, `Clear`, `Resolve`, `BeginTiling`, `EndTiling`, `SyncRing`, `ResyncRing`, `ApplyLoadAluConstants`, `NoteRingConstants`, `InvalidateGuestRange`, `OnSwap`). Expor `using PacketSink = std::function<bool(graphics::guest::RenderPacket&&, std::string&)>`, `bool InstallPacketSink(PacketSink,std::function<void()>)` e `void ShutdownWorker()` no frontend. `Renderer` D3D12 conserva um adaptador para as mesmas operações, sem duplicar a captura.

Leitura validada: `bool ReadGuestBytes(rex::memory::Memory&, uint32_t address, size_t length, std::vector<uint8_t>& out, std::string& error)` em novo `native_renderer/guest_reads_android.h/.cpp`. Copiar após validar intervalo e mapeamentos; falha deixa `out` vazio. Consultar a API de memória real do SDK, incluindo aliases físicos, sem converter qualquer endereço em ponteiro arbitrário.

- [ ] Criar testes `rejects_overflow_and_unmapped_ranges`, `physical_alias_reads_same_bytes`, `published_packet_survives_guest_mutation`, `preserves_clear_float4_and_resolve_pitch` e `packet_order_matches_pc_fixture`. Usar fixtures sintéticas pequenas com sequência clear/draw/resolve/swap e assertar valores e ordem dos pacotes.
- [ ] Executar `tools/test_native.ps1 -Suite frontend -Device RXCWB05KQMX`; esperar falha por frontend ainda acoplado ou API ausente.
- [ ] Extrair as operações completas para frontend portátil. Atualizar hooks para chamar o frontend ativo; manter recursos, constantes, shaders e texturas capturados com a posse já usada no PC. Preservar chamadas originais exigidas para flush de estado e evitar dupla emissão de DrawVerticesUP.
- [ ] Implementar leitura Android e erro contextual por endereço/tamanho/operação. Não portar `ReadProcessMemory`, cabeçalhos COM ou caches DXIL para o alvo Android.
- [ ] Rodar a suíte `frontend`, testar cancelamento do packet sink e compilar o alvo frontend ARM64 isolado. Aplicar o patch numa cópia PC de verificação e executar as suítes nativas pertinentes para conferir preservação da captura PC; não editar o checkout PC original.
- [ ] Commitar: `refactor: share native capture frontend with Android Vulkan`.

### Tarefa 4: Biblioteca de shaders utilizável sem ferramentas Windows no aparelho

**Files:** criar `tools/prepare_native_shaders.ps1`, `tools/native-patches/android-shader-library.patch`, `tests/native/test_android_shader_library.cpp`; modificar CMake/runner. Referências: `PC/tools/shaders/build_corpus.ps1`, `verify_vulkan_preshaders.py`, `PC/port/src/graphics/shaders/vulkan_shader_service.*`, `PC/port/src/graphics/vulkan/platform/shader_process_android.*`.

**Interfaces:** `prepare_native_shaders.ps1 -RecompRoot <PC> [-Install -Device RXCWB05KQMX]` gera/verifica `superman_returns_vulkan.srvk` com as ferramentas PC existentes, registra SHA-256 e instala em `files/shaders/superman_returns_vulkan.srvk` via staging e `run-as org.supermanreturns.mobile`. Sem `-Install`, só prepara no host. Não desinstalar o aplicativo nem perder saves.

Adicionar `bool precompiled_only=false` e `std::filesystem::path missing_dump_dir` a `VulkanShaderConfig`; Android usa `true` e diretório privado. Preservar `Request`, `Poll`, `PrecompiledCount`, `LibraryDiagnostic` e formato `.srvk` existentes. Gravar `.bin` e metadados de hash/estágio em `missing_dump_dir` quando faltar shader. Processo Windows nunca é chamado em modo precompiled-only.

- [ ] Criar `loads_known_shader_without_process`, `missing_shader_fails_with_recoverable_container`, `rejects_empty_or_truncated_library` e `rejects_wrong_stage_or_incompatible_artifact`: assertar ready/failed, diagnóstico útil, dump recuperável e zero chamadas ao runner de processos.
- [ ] Rodar `tools/test_native.ps1 -Suite shaders`; esperar falha pelo modo precompiled-only ausente.
- [ ] Implementar modo e ferramenta de preparação. Usar tanto shaders extraídos quanto os dumps de runtime PC disponíveis, verificando cobertura; não afirmar que todos os shaders de todas as fases estão cobertos.
- [ ] Remover, no provider Android privado, a exigência de arquivos Python/DXC/emitter para esse modo. Falha da biblioteca bloqueia inicialização; falha de shader usado no quadro é propagada ao bootstrap para parar o guest e informar o motivo.
- [ ] Rodar a suíte `shaders`, gerar a biblioteca real e executar o verificador PC existente. Registrar versão de tradutor e SHA-256; confirmar que dados derivados permanecem ignorados pelo Git.
- [ ] Commitar: `feat: load native Vulkan shader library on Android`.

### Tarefa 5: Sistema nativo de comandos, MMIO e progresso

**Files:** criar `tools/native-patches/native-command-system.patch`, `tests/native/test_native_commands.cpp`; modificar CMake/runner. Referências: `PC/port/src/native_renderer/native_graphics_system.*`, `native_graphics_system_interface.h`, `native_graphics_system_vulkan.*`, `native_bridge.*`, `game_profile.h`.

**Interfaces:** implementar `VulkanNativeGraphicsSystem` com os métodos reais de `IGraphicsSystem` da tarefa 2 e `INativeGraphicsSystem`, incluindo `uint64_t progress_generation() const`, `void SignalGpuProgress()`, `void WaitProgress(uint64_t,uint32_t)`, `uint32_t guest_frame_counter() const`, `bool GetGammaRamp256(uint32_t*) const` e `void WaitCondition(const std::function<bool()>&,uint32_t)`. Usar o processador de comandos nativo PC extraído para novo `native_command_system.h/.cpp`; provider passa por injeção, sem DXGI.

- [ ] Criar `ring_wrap_preserves_big_endian_packets`, `writeback_and_identifier_publish_progress`, `interrupt_keeps_callback_arguments`, `wait_wakes_on_progress_or_shutdown`, `shutdown_is_idempotent` e `paused_guest_does_not_advance_vblank`. Assertar efeitos observáveis na memória guest e ausência de wait bloqueado após cancelamento.
- [ ] Rodar `tools/test_native.ps1 -Suite commands -Device RXCWB05KQMX`; esperar falha contra a implementação vazia.
- [ ] Extrair do PC Reader/ExecuteBuffer/ExecutePacket/ExecuteType3, MMIO, interrupções, threads de comandos e vblank. Preservar semântica dos pacotes e limites de recursão; nenhum retorno fixo de sucesso substitui trabalho necessário.
- [ ] Implementar ciclo de vida e cancelar frontend/worker antes de destruir recursos. Expor progress helpers pela instância ativa do contrato nativo, sem ponteiro global tipado como D3D12.
- [ ] Rodar a suíte `commands` e a suíte `contract` com o objeto nativo real. Conferir que o encerramento acorda todos os waits e que identificador de progresso funciona com o SDK Android, mesmo quando não tinha equivalente no PC.
- [ ] Commitar: `feat: connect native GPU commands and progress on Android`.

### Tarefa 6: Provider Android e core Vulkan PC

**Files:** criar `tools/native-patches/android-provider-core.patch`, `tests/native/test_native_provider.cpp`; modificar CMake/runner. Referências: `PC/port/src/graphics/vulkan/CMakeLists.txt`, `platform/android_provider.cpp`, `platform/native_provider.h`, `loader.cpp`, `game_frame.*`, `frame_loop.*`, `resources.*`.

**Interfaces:** preservar `std::unique_ptr<rex::ui::GraphicsProvider> CreateNativeVulkanProvider(NativeProviderConfig)` e o sink usado pelo PC. Android usa `rex::ui::AndroidNativeWindowSurface`, `Surface::kTypeIndex_AndroidNativeWindow` e flags reais definidos em `surface.h`, sem os nomes inexistentes `AndroidWindowSurface`. `VulkanNativeGraphicsSystem::SetupPresentation` recebe o provider produzido e publica seu presenter pelo contrato da tarefa 2.

- [ ] Criar testes `provider_requires_valid_shader_library`, `rejects_missing_required_device_feature`, `surface_recreation_retires_pending_images`, `device_loss_stops_submissions` e `driver_cache_identity_mismatch_discards_cache`. Usar mocks existentes das suítes Vulkan para a posse por fence e executar apresentação real no S22 no smoke de integração.
- [ ] Rodar `tools/test_native.ps1 -Suite provider -Device RXCWB05KQMX`; confirmar falha por tipos Android/dependências Windows ou comportamento ausente.
- [ ] Corrigir provider para incluir `surface_android.h`; configurar loader Android, extensões e features usadas pelos shaders e resolves. Reportar o nome da feature exigida se a GPU não oferecer suporte, em vez de habilitá-la sem consulta.
- [ ] Ajustar CMake para cross-compile Android: `VK_USE_PLATFORM_ANDROID_KHR`, `VK_NO_PROTOTYPES`, link Android/log/dl e Vulkan via loader existente. Compilar shaders auxiliares de depth resolve, alias, composição e UI com DXC do Windows no host, selecionado por caminho explícito; não usar `find_program(dxc)` do ambiente alvo como se fosse executável Android.
- [ ] Integrar suspensão, desconexão e recriação de superfície ao mecanismo do presenter; aguardar recursos pendentes por fence. Separar cache do driver por UUID, versão de driver e versão de conteúdo; cache corrompido é descartado e reconstruído.
- [ ] Rodar suíte `provider`, smoke real de apresentação/recriação no S22 e suítes pertinentes de resolve/composição/recursos do core. O smoke valida a infraestrutura, não substitui gameplay na tarefa 8.
- [ ] Commitar: `feat: present PC native Vulkan frames on Android`.

### Tarefa 7: Conectar hooks, factory, APK e controles

**Files:** criar `native/game/native_bootstrap.h/.cpp`, `tests/native/test_native_bootstrap.cpp`; modificar `native/game/main.cpp`, `native/game/hooks.cpp`, `native/game/CMakeLists.txt`, `tools/build_game.ps1`, `tools/native-patches/native-command-system.patch`, `android/app/src/main/java/org/supermanreturns/mobile/GameActivity.java` e `ControllerView.java` apenas no que os testes exigirem. `GameActivityControllerView` é classe interna de `GameActivity.java`, não um arquivo separado.

**Interfaces:** bootstrap em `superman_returns::android`: `std::unique_ptr<rex::system::IGraphicsSystem> CreateNativeVulkanGraphicsSystem(const std::filesystem::path& files_root)`; configura biblioteca `files_root/shaders/superman_returns_vulkan.srvk`, cache e dumps privados, modo precompiled-only e callbacks de erro. Logs de boot incluem `renderer=pc-native-vulkan`, revisão de fontes e hash da biblioteca; estatísticas incluem hooks, pacotes, draws e resolves.

Na bridge PC privada, acrescentar `bool ActivateNativeFrontend(NativeFrontend&, std::string& error)` e `void DeactivateNativeFrontend()`: publicar a instância somente após instalação do sink e preparação da apresentação, controlar `RendererActive()` e encaminhar `OnFrameStatsSwap` ao frontend. Desativação ocorre antes da destruição. Essas funções substituem a dependência de inicializar `Renderer::Get()` D3D12; a factory Android não chama a seleção Xenos/A-B da bridge PC.

- [ ] Criar `missing_library_blocks_guest_resume`, `factory_returns_native_system`, `sink_failure_stops_guest_without_fallback`, `swap_forwarded_exactly_once` e `focus_loss_clears_all_touch_buttons`. Assertar que não existe objeto Xenos no caminho escolhido; usar chamadas reais do bootstrap e bridge com dependências fake, sem testar somente strings de log.
- [ ] Rodar `tools/test_native.ps1 -Suite bootstrap -Device RXCWB05KQMX`; esperar falha pela factory atual.
- [ ] Conectar frontend ativo, hooks e provider ao sistema da tarefa 5 antes do Resume. Compilar `native_hooks.cpp` e dependências na versão adaptada; no hook `sub_82112050` preservar original e encaminhar exatamente uma vez `OnFrameStatsSwap` com os argumentos de entrada corretos. Resolver duplicidades de símbolos de hooks no link, sem remover hooks necessários.
- [ ] Trocar `OnPreSetup` para o bootstrap nativo; retirar argumentos que configuram exclusivamente emulação Xenos. Preservar ponte JNI de touch, áudio/XMA, saves e importação. Mensagem de falha deve chegar ao launcher/log acessível; não deixar o usuário diante de uma janela preta sem diagnóstico.
- [ ] Rodar todas as suítes nativas afetadas e `tools/build_game.ps1`, `tools/build.ps1`; exigir build, lint, APK verify, assinatura e zipalign 16 KB aprovados. Instalar com `adb install -r`, preservando dados, e instalar a biblioteca pelo script da tarefa 4.
- [ ] Abrir o jogo no S22; exigir logs nativos, progresso, pacotes e draws reais. Exercitar dois sticks simultâneos, botões de voo/ataque/pausa e soltura ao perder foco. Conferir áudio sem alterar ASTs.
- [ ] Commitar: `feat: launch Superman with PC native Vulkan on S22`.

### Tarefa 8: Validar gameplay, corrigir diferenças e registrar entrega

**Files:** criar `tools/validate_native_device.ps1` e `docs/evidence/s22-native-vulkan-*`; modificar `docs/validation.md`, `docs/android-port.md`, `THIRD_PARTY_NOTICES.md` e patches das tarefas responsáveis por defeitos encontrados. Logs e dumps de dados do jogo permanecem privados em `.tools`.

**Interfaces:** `validate_native_device.ps1 -Device RXCWB05KQMX -DurationSeconds 600 -OutputDir <dir>` coleta por PID de `org.supermanreturns.mobile:game`, memória a cada 60 segundos, contadores de quadro e exit-info do próprio pacote. Gera `summary.json` com duração, revisões, hashes, amostras de PSS/Graphics, FPS/tempos de quadro, status do processo e evidências. Não encerra outros aplicativos nem captura logs pessoais amplos.

- [ ] Fazer um teste curto do coletor com o processo parado: exigir erro claro, sem relatório de gameplay aprovado. Conferir também PID alterado durante coleta: marcar sessão interrompida e não juntar duas execuções como uma.
- [ ] Capturar a cena PC com Vulkan nativo e o mesmo ponto no S22. Comparar personagem, cenário, texturas, luz, profundidade, pós-processamento e HUD. Usar dumps intermediários para corrigir o primeiro passo divergente; não considerar HUD sobre preto como aprovação.
- [ ] Executar roteiro no S22: iniciar jogo novo, mover personagem e câmera, voar, pousar, realizar combate disponível na área inicial, pausar, ir ao fundo, retornar e continuar. Registrar ações e screenshots correspondentes. Se o combate exigir avanço, completar o trecho necessário antes de marcar o critério.
- [ ] Rodar sessão de 600 segundos, amostrando memória a cada minuto. Repetir a mesma rota para investigar crescimento contínuo; corrigir retenções antes de aceitar. Confirmar ausência de crash, encerramento por memória, input preso e cena permanentemente preta.
- [ ] Medir primeira execução e cache aquecido no mesmo roteiro. Reportar FPS, tempos de quadro, picos e diferenças para PC; não tratar menos de 30 FPS como paridade de desempenho. Evitar otimização por perda silenciosa de draws, shaders ou efeitos.
- [ ] Reexecutar testes afetados pelas correções. Numa cópia PC de verificação, aplicar adaptações compartilhadas e compilar o executável PC e suítes pertinentes; repetir a referência visual se houve alteração da captura ou do core. Preservar o checkout PC original.
- [ ] Atualizar documentação, avisos e relatório com hashes do APK e shader library, comandos de build, resultados reais e limitações. O critério funcional só é PASS após cidade, voo e combate observados; a campanha inteira permanece não certificada.
- [ ] Commitar evidências e documentação sem dados privados: `test: validate native Vulkan gameplay on Galaxy S22`.

## Autorrevisão do plano

- Cobertura: fontes/contrato/captura/shaders/comandos/provider/APK/controles/validação correspondem às tarefas 1–8 e a todas as seções da especificação.
- Interfaces: o contrato comum fica no SDK; `INativeGraphicsSystem` adiciona somente funções nativas; frontend produz `RenderPacket`, provider mantém a factory PC e bootstrap é o único ponto de seleção Android.
- Proveniência: adaptações entregues como patches mobile; nenhum passo depende de editar o checkout PC ou de commitar bibliotecas derivadas do jogo.
- Os cinco riscos de Review Focus têm testes nas tarefas proprietárias. Device loss e cache incompatível também têm verificações na tarefa 6.
- Smoke, build e contagem de swap são marcos intermediários. A entrega funcional exige o roteiro completo da tarefa 8; paridade de FPS e campanha não são presumidas.

## Handoff

Recomendação: execução Native nesta conversa, pois frontend, contrato do SDK,
comandos e hooks dependem fortemente uns dos outros. O mesmo executor mantém
essas relações enquanto implementa e testa; a revisão independente ocorre no
fim, conforme o método da skill. A alternativa é Subagent-driven, com executor
e revisor separados por tarefa, maior isolamento e maior custo de contexto.

Aguardar revisão deste plano e escolha do método pelo usuário antes de alterar
código de produto ou iniciar a implementação.

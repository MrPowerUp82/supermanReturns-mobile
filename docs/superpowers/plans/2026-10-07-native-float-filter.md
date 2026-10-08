# Native Float32 Filtering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Superar a falha de filtragem R32_SFLOAT no S22 preservando precisão float32 e o estado de amostragem solicitado pelo jogo.

**Architecture:** Acrescentar amostragem 2D por shader ao core Vulkan PC privado, selecionada somente quando o formato não oferece a filtragem requerida. Versionar em conjunto o contrato de bindings, os resultados compilados e a biblioteca privada; ativar a correção somente depois do teste numérico no GPU.

**Tech Stack:** C++23, Vulkan 1.1, HLSL/DXC host, Python, PowerShell, Android NDK27.2.12479018, ARM64, Galaxy S22 Adreno730.

**Spec:** `docs/superpowers/specs/2026-10-07-native-float-filter-design.md`, aprovada nesta conversa.

## Global Constraints

- Reutilizar o core Vulkan nativo do PC; checkout PC original preservado.
- Texturas permanecem float32. Gamma depois da filtragem; endereçamento preservado.
- ASTs, layout guest 1280×720 e efeitos preservados; nenhum draw ou shader utilizado é ignorado.
- Compilação de shaders no host; biblioteca derivada do jogo permanece privada.
- Bibliotecas antigas/incompatíveis são rejeitadas antes de iniciar o guest.
- Estados sem equivalência implementada continuam falhando explicitamente.
- Instalação de teste `org.supermanreturns.mobile.native`; preservar pacote original e dados.
- Gameplay só passa após os critérios originais de cidade, voo, combate, áudio, pausa/retomada e 600 segundos.

## Review Focus

1. Atualizar somente APK ou biblioteca deve falhar antes de Resume — tarefa 1, `rejects_v1_and_mixed_library`.
2. Cache de descriptors pode confundir filtros ou recursos novos — tarefa 3, `filter_metadata_separates_cache_entries`.
3. Coordenadas fora da imagem e mips de dimensões ímpares devem preservar endereçamento — tarefa 2, `gpu_edges_and_odd_mips`.
4. Valores próximos que colapsam em float16 devem continuar distintos — tarefa 2, `gpu_preserves_float32_precision`.
5. Mip base-map, filtros mistos e estados não implementados não podem virar nearest silenciosamente — tarefas 2/3, `gpu_min_mag_and_mip_selection` e `unsupported_filter_reports_context`.

## Decisões do contrato

Revisão 2: `BindingContractVersion=2`, ABI `sr-vulkan-buffers-v2`.
Preservar os layouts, offsets e arrays de 32 descriptors existentes. Cada word de
sampler no bloco shared usa bits 0–4 como índice real, bit 5 como seleção do helper
manual, bit 6 para mag linear, bit 7 para min linear, bits 8–9 para mip filter.
Bits 10–31 são reservados e zero. Os helpers sempre mascaram o índice por `31u`.
Índices de texturas, gamma e escala mantêm o contrato existente.

Enums da fonte fixada `rex/graphics/xenos.h`: mip point=0, linear=1, base-map=2;
3 é use-fetch-constant e não constitui estado final resolvido. Base-map amostra
nível 0. O primeiro caso real tem mag=min=linear, mip=base-map, aniso=0.
O sampler de taps usa min/mag/mip nearest e anisotropia desativada; conserva o
endereçamento original. A interpolação efetiva é calculada no shader.

O header SRVKLIB e a revisão do resultado SVR3 passam de 1 a 2. Mudança unilateral
é rejeitada. O nome ABI e hashes entram na identidade do cache; cache v1 não é
reutilizado. Nenhum pacote com o novo core é instalado antes da biblioteca v2
estar verificada e o teste GPU estar aprovado.

## Mapa de arquivos

- `tools/prepare_native_shader_tools.py`: snapshot privado das ferramentas/common PC com hashes e patches.
- `tools/shader-patches/float-filter-v2.patch`: adaptações das ferramentas e common; pasta distinta dos patches do renderer.
- `native/shaders/float_filter.hlsl`: helper de produção incorporado ao common preparado, incluído também no teste GPU.
- `tools/native-patches/zzzzz-float-filter.patch`: contrato, loader, planejamento e descriptors do core privado.
- `tests/test_prepare_native_shader_tools.py`: staging, incompatibilidade e rastreabilidade.
- `tests/native/test_texture_filter.cpp`: planejamento, metadata e caches.
- `tests/native/test_float_filter_gpu.cpp` e `tests/native/shaders/float_filter_probe.hlsl`: teste Vulkan real/readback e referência CPU independente.
- `native/renderer-tests/CMakeLists.txt`, `tools/test_native.ps1`: compilar e executar suíte `filter` no S22.
- `tools/prepare_native_shaders.ps1`: consumir ferramentas adaptadas e publicar biblioteca v2 verificada.
- `tests/native/test_android_shader_library.cpp`, `test_native_bootstrap.cpp`: rejeição de ABI e ordem de startup.
- `docs/evidence/s22-native-vulkan-*`, `docs/validation.md`, checkpoint: resultados reais e limitações.

### Tarefa 1: Biblioteca v2 e preparação rastreável

**Interfaces:** `prepare_native_shader_tools.py --recomp <PC> [--output <dir>] [--verify]`, saída padrão `.tools/native-shader-tools`, `source-lock.json` com revisão, hashes de entradas, patches, helper e saídas. Expor `LIBRARY_VERSION=2` nas ferramentas privadas e usar `BindingContractVersion` no loader privado. `VulkanShaderConfig` permanece precompiled-only no Android.

- [x] Escrever `test_v2_tool_snapshot`, `test_verify_detects_helper_change`, `test_incompatible_patch_preserves_previous_output`: verificar versões, hashes, detecção de alteração e staging atômico.
- [x] Executar `python -m unittest discover -s tests -p test_prepare_native_shader_tools.py`; registrar RED por ferramenta ausente.
- [x] Implementar preparação copiando `tools/shaders/*.py` e o common existente para árvore privada; aplicar patch versionado. Incorporar o conteúdo completo do helper ao common para que seu hash cubra o código consumido pelo compilador. Recusar fonte/destino sobrepostos e publicar somente após validação.
- [x] Adaptar as cópias de `vulkan_contract.py`, `runtime_vulkan_shader.py`, `make_vulkan_preshaders.py`, `verify_vulkan_preshaders.py`: ABI, revisão 2 e argumento de estágio quando necessário. Preservar a validação independente por reflexão e checksums; não apenas editar o header de um artefato v1.
- [x] Escrever `rejects_v1_and_mixed_library`: rejeitar header v1, resultado v1 dentro de header v2, estágio errado, truncamento e metadata incompatível. Atualizar fixtures válidas para v2.
- [x] Rodar testes de shaders/bootstrap e registrar RED; modificar `binding_contract.h` e `vulkan_shader_service.cpp` via patch privado para exigir revisão 2. Garantir que biblioteca incompatível bloqueia a factory antes de Resume.
- [x] Rodar testes Python e `tools/test_native.ps1 -Suite shaders -Device RXCWB05KQMX`, `-Suite bootstrap`; exigir GREEN. Preparar/`--verify` snapshots e conferir checkout PC intacto.
- [x] Commitar somente arquivos desta unidade após a verificação, sem ferramentas privadas ou dados do jogo.

### Tarefa 2: Helper float32 com prova numérica no GPU

**Interfaces HLSL:** `float4 srFilter2DLevel(Texture2D<float4> texture, SamplerState taps, float2 uv, uint level, bool linear)`; `float4 srFilter2D(Texture2D<float4> texture, SamplerState taps, float2 uv, float lod, uint samplerWord)`. O helper de nível é puro quanto a LOD; a função externa escolhe min/mag e mip conforme metadata. A wrapper de produção `tfetch2D` fornece LOD não clamped e UV com o offset/escala atuais antes de chamar o helper.

**Interfaces de teste:** executável `test_float_filter_gpu`, suíte `filter`. O teste cria instance/device Vulkan headless via loader, seleciona fila com suporte a compute/graphics e não depende de superfície Android. Usa o mesmo helper de produção em um shader de probe com LOD explícito e uma passagem fragment offscreen para testar a consulta de LOD da wrapper. Readback em buffer host-visible depois de fence com timeout de 5 segundos; referência CPU usa double, indexação e pesos próprios.

- [x] Criar o teste GPU: consultar propriedades reais de R32_SFLOAT, registrar falta de filtro linear e reproduzir RED da seleção antiga. Não considerar ausência de aparelho como PASS.
- [x] Criar casos `gpu_preserves_float32_precision`, `gpu_edges_and_odd_mips`, `gpu_min_mag_and_mip_selection`. Usar textura 5×3 com mip chain, valores como `1.0001220703125` e `1.000244140625` e valores negativos. Amostrar centros, pesos de 0.5 e 0.25, coordenadas negativas/maiores que 1, clamp-edge, repeat, mirrored-repeat e border zero.
- [x] Comparar cada componente com tolerância absoluta `2e-6 * max(1, abs(expected))`; exigir que os valores próximos permaneçam distintos. Asserções de nível exato para base-map e ponto; LOD fracionário para mip linear. Exercitar as duas combinações min/mag mistas em LOD positivo e negativo.
- [x] Compilar/executar a suíte e registrar RED antes do helper. O runner deve propagar falha de upload, shader compile, VkResult, timeout e comparação numérica.
- [x] Implementar taps nos centros dos texels: para filtro linear, partir de `uv * dimensions - 0.5`, consultar quatro texels com o sampler pontual e combinar em float32. Para ponto, uma amostra. Consultar dimensões do mip efetivo, preservar modos de endereço e clamp dos níveis válidos.
- [x] Implementar mip point por `floor(clamp(lod, 0, levels-1) + 0.5)`, linear pelos dois níveis vizinhos e o peso fracionário, base-map pelo nível 0. LOD positivo seleciona min; zero ou negativo seleciona mag. Preservar offset e conversão gamma depois da interpolação. Operações que precisam de derivadas só podem ser usadas nos estágios que as suportam; compilação inválida deve falhar, nunca substituir por LOD inventado.
- [x] Na cópia common, selecionar o helper pelo bit 5 e mascarar todas as referências à array de samplers. Caminho de hardware continua usando os filtros originais. Configurações de bicubic/escala incompatíveis com a prova atual devem ser rejeitadas explicitamente na tarefa 3.
- [x] Rodar `tools/test_native.ps1 -Suite filter -Device RXCWB05KQMX`; exigir readback GREEN, resultados registrados e zero erro de validação quando a camada estiver disponível. Ausência da camada deve ser registrada.
- [x] Commitar helper, probe, testes e runner após GREEN.

### Tarefa 3: Seleção no core e coerência dos descriptors

**Interfaces C++:** novo `graphics/vulkan/texture_filter.h/.cpp` privado com `TextureSamplingPlan { uint32_t sampler_word; std::array<uint32_t,6> sampler_fetch; bool manual; }` e `bool PlanTextureSampling(std::span<const uint32_t,6> fetch, VkFormat format, VkFormatFeatureFlags features, TextureDimension dimension, uint32_t slot, TextureSamplingPlan& out, Error&)`.

`DescriptorStore::Prepare(DrawBindings& bindings, ...)` prepara metadata antes de Shared/flush e escreve os words no shared offset `128 + slot*4`. A assinatura restante permanece igual. `Sampler` recebe o fetch efetivo do plano; a chave e a validação de índices devem considerar metadata e o sampler efetivo.

- [x] Escrever `float32_2d_selects_manual_when_linear_missing`, `filterable_formats_keep_hardware`, `basemap_uses_level_zero`, `unsupported_filter_reports_context`: formato/dimensão/slot/filtros presentes no erro, saída inalterada ao falhar.
- [x] Cobrir estados finais: mag/min 0 ou 1; mip 0,1,2; aniso 0 ou 1 sem anisotropia efetiva. Para manual, permitir somente R32_SFLOAT/R32G32_SFLOAT/R32G32B32A32_SFLOAT em 2D. Rejeitar aniso>1, estado use-fetch não resolvido, dimensões não cobertas, formato não comprovado e flags de efeito incompatíveis.
- [x] Rodar a suíte e registrar RED. Implementar o plano sem converter a imagem. O sampler de taps mantém endereço e border, usa filtros nearest e desativa anisotropia; os words guardam os filtros efetivos do guest.
- [x] Integrar em Prepare usando consulta de propriedades por formato em cache. Verificar metadata antes da escrita dos descriptors; hardware conserva o fetch original. Propagar falhas pelo diagnóstico existente.
- [x] Escrever `filter_metadata_separates_cache_entries`: mesmos recursos com filtros diferentes não reutilizam o sampler errado; cache hit e miss produzem os mesmos words; textura substituída por formato diferente é reavaliada. Testar índice 31, bits reservados, dummy/non-texture e todos os slots.
- [x] Rodar `filter`, `provider`, `shaders`, `bootstrap` e suites afetadas de frontend/commands no S22. Atualizar fixtures privadas existentes que assumem revisão 1, sem afrouxar rejeições. `prepare_native_renderer.py --verify` deve passar.
- [x] Commitar patch e testes verificados; não instalar o core isoladamente.

### Tarefa 4: Biblioteca completa, APK e retorno à validação funcional

**Interfaces:** `prepare_native_shaders.ps1 -RecompRoot <PC> -Install -Device RXCWB05KQMX -Package org.supermanreturns.mobile.native` usa o snapshot v2. Registrar hash das ferramentas, helper, common, ABI, fonte e biblioteca; runtime inicia somente com biblioteca compatível.

- [x] Integrar preparação privada no script de shaders, incluindo revisão do contrato na proveniência e identidade do cache. Gerar biblioteca inteira com as ferramentas PC copiadas/adaptadas e verificar independentemente. Zero falha de shader utilizado; não usar `-AllowIncomplete` para aprovar boot.
- [x] Build/link completo `tools/build_game.ps1 -RecompRoot <PC>` e `tools/build.ps1 -NativeSideBySide`; exigir lint, assinatura, zipalign e ELF16KB. Confirmar PC sem alterações.
- [x] Instalar APK com `adb install -r` e biblioteca v2. Abrir launcher, localizar botão pelo estado atual da UI e repetir o boot. Registrar se o pacote 34 foi superado e diagnosticar a primeira falha nova antes de alterar outro comportamento.
- [ ] Reexecutar referência visual PC versus S22 e roteiro original: cidade, movimento/câmera, voo/pouso, combate, áudio, pausa/fundo/retorno. Comparar imagem antes de otimizar.
- [ ] Coletar 600 segundos com `tools/validate_native_device.ps1 -Device RXCWB05KQMX -Package org.supermanreturns.mobile.native -DurationSeconds 600 -OutputDir .tools/native-float-validation`; investigar crescimento de memória e medir execução fria/aquecida. Contador de swaps não é timing de apresentação.
- [ ] Executar revisão independente do conjunto final conforme o método Native já escolhido; corrigir problemas com testes. Registrar hashes, resultados e limitações em evidências e checkpoint. Não chamar boot, readback ou HUD de gameplay PASS.
- [ ] Commitar resultados sem logs/dumps privados nem biblioteca derivada do jogo.

## Referências técnicas

- [Regras Vulkan de amostragem](https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/chapters/textures.adoc): LOD, endereçamento e interpolação.
- [Consulta HLSL de LOD não clamped](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-to-calculate-lod-unclamped).
- Fonte local fixada `rex/graphics/xenos.h`: significado de base-map e enum de filtros.

## Autorrevisão e handoff

Cobertura: tarefa 1 protege ABI/proveniência e startup; tarefa 2 prova matemática
e precisão no GPU; tarefa 3 seleciona o caminho sem perda silenciosa; tarefa 4
integra e retoma todos os critérios funcionais originais. Os cinco Review Focus
têm testes nas tarefas proprietárias. A nova semântica fica confinada ao word de
sampler e à revisão v2, sem novos descriptors ou alterações de dados guest.

Método preservado: Native, implementação nesta conversa. Plano aprovado pelo usuário nesta conversa após a aprovação da especificação.
Tarefas1–3 concluídas; tarefa4 em validação no aparelho. Código commit c082611.

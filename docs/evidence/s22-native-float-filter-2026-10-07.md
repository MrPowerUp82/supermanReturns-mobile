# Filtro float32 nativo no S22 — 2026-10-07

## Resultado desta etapa

Core Vulkan nativo do PC integrado ao Android com contrato de bindings v2 e
filtragem 2D float32 por shader quando o formato não oferece filtro linear.
A matemática usa taps pontuais e interpolação float32; imagens não são
convertidas para float16. A biblioteca v2 deve acompanhar o novo APK.

APK659c0067 e biblioteca v2 instalados. O boot PID1565 chegou a Metropolis
com camada Khronos carregada; movimento, câmera, voo, pouso, pausa e retorno
do app observados. Cores e orientação corrigidas. Combate e qualidade do áudio
ainda não aprovados; coleta600s em andamento. O crash descrito a seguir pertence
à build anterior e foi superado pelos patches locais.

Em 2026-10-08, após restauração autorizada do pacote e dados, LLDB capturou uma
exceção não tratada: leitura de endereço host 0x18 em libllvm-glnext.so, dentro
da compilação Qualcomm chamada por vkCreateGraphicsPipelines, frame serial 89.
O par SPIR-V extraído da memória passou no spirv-val Vulkan1.1. Isso localiza a
falha. A reprodução isolada confirmou que um VkPipelineCache compartilhado
dispara a falha ao compilar variantes da máscara de escrita de cores. Cache
fresco: variante0 compila, variante1 cai. Com VK_NULL_HANDLE:64 variantes passam.
O mesmo teste usando GamePipelineStore real reproduziu RED(exit139) e passou
após a correção, incluindo64 reutilizações de objetos de pipeline.

O patch zzzzzz-driver-cache.patch desabilita somente esse cache de compilação
no Android vendor0x5143/device0x07030001/driver0x80267062. Mantém o cache de
objetos GamePipeline e todos os draws/efeitos. Outros drivers não são alterados.
Uma sessão com camada Khronos1.4.363.0 carregada também caiu no driver sem VUID
observado antes da falha; não equivale a gameplay validada.
Uma captura anterior de escrita guest B782B0B8 era um watch tratado, não a
exceção fatal: breakpoint na linha 315 também pegava o epílogo normal otimizado.
Usar breakpoint por nome DispatchUnhandledSignal para capturar somente falhas
não tratadas. Logs/dumps privados: .tools/filter-debug4-{unhandled,pipeline}.txt.

## Evidência verificável

- S22 SM-S901E, Android16, Adreno730: 34.560 comparações numéricas PASS em
  R32/RG32/RGBA32 float32. Texturas 5×3, mips ímpares, valores próximos que
  colapsariam em float16, endereçamento repeat/mirror/edge/border, filtros
  min/mag mistos, mip point/linear/base-map e LOD explícito/implícito.
- Seleção CPU em todos os 32 slots e filtros finais: PASS. Estados manuais não
  cobertos (anisotropia, mirror-clamp, 3D/cube, outros formatos) falham explicitamente.
- Descriptors reais no GPU: cache hit/miss, mudança de filtros, troca de formato,
  índice31, bits reservados e flags bicubic incompatíveis: PASS.
- Sete suítes de componentes no aparelho: filter, provider, shaders, bootstrap,
  frontend, commands e contract PASS. Provider inclui 88 testes Vulkan, zero
  checks falhos. Uma interrupção ADB em frontend foi seguida de reexecução PASS.
- Camada de validação Vulkan indisponível no aparelho; não há alegação de PASS
  dessa camada. O teste solicita a camada e informa sua ausência.
- Biblioteca inteira: 241/241 shaders prontos, zero falhas, verificador
  independente PASS. v1 e resultados mistos são rejeitados antes da factory.
- Build/link ARM64, assemble, lint, assinatura, zipalign e ELF16KB PASS.
- Importador: 15 testes PASS com o jogo local. Testes Python: 21 PASS.
- Revisão independente: base-map em hardware foi corrigido para maxLod=0 após
  teste RED no aparelho; teste de LOD implícito ampliado após revisão. Nenhum
  problema acionável restante encontrado na revisão dessas mudanças.
- Checkout PC original permaneceu limpo; ASTs e layout guest1280×720 preservados.

A referência CPU de filtragem usa double e indexação própria. O teste fragment
mede também o LOD bruto em uma passagem separada, verifica sua diferença para
log2 analítico contra `mipmapPrecisionBits`, e usa esse LOD na referência CPU.
A tolerância da interpolação permanece `2e-6 * max(1,abs(expected))`.
Isso respeita a [precisão de LOD permitida pelo Vulkan](https://docs.vulkan.org/spec/latest/chapters/textures.html).

## Identidades dos artefatos privados

- PC: ae5107814a5686de5fa1eff11a1120d07fa7d883.
- Source-lock renderer SHA256 após quirk de cache: 8586c25e2078b8113e09544802758d41b8573721208f368a0bfebee8150b488a.
- Helper SHA256: 3c8e79510b6a192aca570458fd4973904eb566d8396ebc140165867843bf81bb.
- Shader tools lock SHA256: 5368f665ae39d6ceb70660760f45198dac19eb20802d4e452e99a8f8440741c2.
- Biblioteca SHA256: 36a6269258bc5dc6e0fc1177541993ffb744f773e6f36edaa804645eb3aa4b5a.
- APK SHA256 após quirk de cache: 177fb0c0439dd8403e7fc6554d23c3f743df8b777e9c2db9b365547d23e72134.
- Runtime relinkado SHA256: a892f8b5091220aa88cdd41fc80400e246f1cf2425a18e170e0f5b82f97a10f1.
- ABI: sr-vulkan-buffers-v2; BindingContractVersion/SRVKLIB/SVR3 revisão2.

APK local: `artifacts/superman-returns-native-vulkan-0.1.0-dev.apk`.
Biblioteca privada: `.tools/native-shaders/superman_returns_vulkan.srvk`.
Logs privados: `.tools/filter-final-{filter,provider,shaders,bootstrap,frontend,commands,contract}.log`,
`.tools/filter-game-final.log`, `.tools/filter-apk-final.log`,
`.tools/filter-final-library.log`, `.tools/filter-final-python.log`.
Artefatos derivados do jogo e dumps não são versionados.
O boot fatal descrito acima usava source-lock96a0... e APK e8cbc5...;
o APK177fb0... contém a correção e ainda requer novo boot funcional.

## Regressão do cache do driver

```powershell
./tools/test_native.ps1 -Suite pipeline -Device RXCWB05KQMX -VertexShader .tools/filter-crash-vs.spv -PixelShader .tools/filter-crash-ps.spv
```

Os inputs são SPIR-V privados extraídos da chamada fatal e não acompanham o
repositório. O teste compila64 combinações de máscara de cores/depth test/write
pela implementação real, e confere reutilização do mesmo objeto a cada repetição.
Logs privados filter-cache-regression-{red,green,final}.log. O teste falha se
uma camada ativa reportar erros, mas sua execução headless não encontrou camada;
nenhum PASS de camada é atribuído a esse teste.

## Boot do APK corrigido (2026-10-08)

APK177fb0 instalado por `adb install -r`, dados e biblioteca v2 preservados.
PIDgame27213 passou o ponto fatal e avançou ao loading freeze breath; mais de
44000 draws. O crash LLVM não reapareceu durante essa execução. Ainda sem
PASS de gameplay. Camada Khronos1.4.363.0 carregada externamente confirmou:

- Input-08733: NORMAL1/location6 uint4 no shader versus R32G32B32A32_SFLOAT.
- maxMemoryAllocationCount-04101: 4096 alocações válidas antes da próxima;
  contador live_buffers chegou a4903.

Force-stop após diagnóstico. Settings globais restaurados (enable0, demais
chaves ausentes), camada privada inativa. Log `.tools/filter-cache-boot.log`.
Imagem girada/comprimida: medir capacidades da surface antes de corrigir.

## Próxima execução

Verificar alocação/entrada no novo APK, repetir boot com camada e medir
transform/extent. Depois comparação PC, cidade/movimento/voo/combate/áudio/
pausa/fundo e600segundos. Preservar outros pacotes e checkout PC.

## Regressões de alocação e vertex input

`tools/test_native.ps1 -Suite allocations -Device RXCWB05KQMX`: GPU real
Adreno730,5001valores lidos de volta (incluindo versão substituída retida),
3alocações ao todo sob teto imposto de32. Camada indisponível no headless;
verificação de VUIDs depende do próximo boot Android com a camada externa.

Teste unit:5000IDs sob teto32, offsets alinhados256, append entre submissions
e versões imutáveis. Contrato NORMAL1 usa UINT32 correspondente quando o
shader recebe bits float32; POSITIONfloat continua SFLOAT e DEC3N continua
R32_UINT. Provider90testes/0falhas antes do ajuste final de descriptors.

A revisão identificou que `owner` também representava transientes no cache.
RED da suíte filter: Identical binding missed cache. Flag transient explícita
separa essa política da propriedade do bloco. Após ajuste, cache e34560
comparações float32 passaram novamente. Logs privados device-arena-{gpu,
vertex-green,descriptors-red,filter-green,python}.log; preparador11/11PASS.

## Boot da arena (2026-10-08)

APK08ecf3e02a8a38238598d05b7b5296f61ffd6ae32da72230c264a2b3f5e4b514,
source-lock53a87ec7c9d9765a117e49ccf3900aa496a77b57cb011505509eddd2b8fdcaeb,
runtime1a7eb1de6f2d8146912b8a1ab1b6767336a85e052accf0e7926589a3f0954a32.
Instalação -r preservou dados. PID29550, camada Khronos carregada. Mais de
417501draws/frame359 entre00:36:45 e00:40:24, nenhum VUID ou crash observado.
live_buffers51, live_mb549 (contador de recursos, não PSS). Chegou à pergunta
Start New Game/Load Game; Start touch mantido1s abriu o diálogo. Não é gameplay.
Force-stop para instalar a correção seguinte; log `.tools/device-arena-boot.log`.

Surface medida: requested/current/selected2340x1080, currentTransform2
(ROTATE90), supportedTransforms511. Compositor não prerotaciona. Teste RED
selected_transform2 vsIDENTITY1, GREEN91testes/0falhas após selecionarIDENTITY
quando suportado. [Contrato preTransform](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainCreateInfoKHR.html).
Revisão por inspeção sem achados pendentes; imagem do novo boot ainda necessária.

APK com correção de orientação:530fa078d768c7f724bf2f4e5d85153970f6380c848924831c72d5f3ada94dc0.
Source-lock91019b70ebacb7ef9c05cf797a119799bfca57e3244a329f011ba12fc56adb94.
Runtimeba161112f318f96c176ccd336eda1f3eeaf758847f5ea17e32a11060ecf36a38.
Biblioteca v2 permanece36a6269258bc5dc6e0fc1177541993ffb744f773e6f36edaa804645eb3aa4b5a.
Build/link, assemble/lint, verificadorELF16KB, assinatura e zipalign passaram.

## Orientação e tratamento de suboptimal

APK530fa instalado, PID31317: camada carregada e imagem upright confirmada
no loading Uppercut. pre_transform1, currentTransform2. Porém o loop passou
a recriar a swapchain continuamente (generation1855), pois devolve kRecreate
também para SUBOPTIMAL. Execução interrompida; settings globais restaurados.
Log privado surface-boot.log e screenshot surface-screen.png.

Próxima correção em teste: tolerar SUBOPTIMAL quando a rotação pelo WSI é
deliberada, mantendo OUT_OF_DATE e callbacks reais de resize. Teste cobre
oito presents consecutivos, acquire/present OUT_OF_DATE e device lost.
Não usar APK530fa como build estável ou resultado funcional final.

APK4a2a475c1fa138cb1b6a596ef15d40c5444834d012c866b74ed8cee557ea2aa0
confirmou a causa: PID413, present1000001003 (SUBOPTIMAL), acquireSUCCESS,
intentional_wsi_rotation1, somente generation1 durante a execução observada.
Mais de69916draws/frame208, sem VUID/crash observado. Parado para atualizar gamma.
Logs suboptimal-{red,green,boot}.log; GREEN92testes/0falhas.

## Gamma: regressão e correção no compositor

O layout privado Display usa gamma[0..255] a partir de shared+0. Prepare genérico
escrevia sampler metadata em shared+128, substituindo gamma[32..63]. Teste com
compositor real, gradiente256x1 e LUT assimétrica: RED no índice32, vermelho0
versus32 e verde0 versus223. A LUT e as opções agora são escritas por último,
seguidas por FlushTransient. GREEN256entradas/1024componentes GPU. Camada não
disponível no headless. Revisão independente sem achados pendentes.

APK659c0067bf6b35c0c29256e52c703fb0dc973bc37644ffe8dd3d13c752563e6f
contém a correção; build/verificações passaram. Log composition-gamma-{red,green}.log,
provider final92/0.

Boot gamma em 2026-10-08: instalação -r bem-sucedida, PID1565, camada
VK_LAYER_KHRONOS_validation carregada. Loading Superbreath e tela inicial
confirmados upright, sem a posterização anterior. Diálogo Start New Game
respondeu a START mantido3s; A mantido5s iniciou novo loading. Até esse ponto,
apenas swapchain generation1 e nenhum VUID/crash observado. Capturas privadas
composition-{screen,menu,newgame}.png. Cidade e comparação PC ainda pendentes.
Coleta de600s iniciada em .tools/native-float-validation; resultado ainda pendente
e não certifica gameplay por si só.

Metropolis apareceu após o loading (captura composition-newgame.png). Analógico
esquerdo alterou posição e direito alterou câmera. Y decolou; Superman visto
acima dos prédios (composition-tutorial.png); novo Y desceu e pousou na rua
(composition-landed.png). START abriu Pause Menu; HOME e retorno pela task
preservaram PID1565 e menu, A retomou a cena. Swapchain generation2 corresponde
ao retorno real da surface, sem recriação contínua. Referência PC
bench_native_c3_final_after_vulkan_idle.png comparada qualitativamente: traje,
prédios, árvores e iluminação escura plausíveis; enquadramentos diferentes,
sem comparação pixel a pixel. A cena seguinte mostrou meteoros sobre Metropolis.
Combate com inimigos ainda não verificado. Primeira captura áudio durante pausa
foi silenciosa; não aprova áudio. Segunda captura output durante cena contém
18.82s de áudio não silencioso (média-39.8dB, pico-25.4dB). Isso confirma sinal,
sem aprovar qualidade auditiva. FFmpeg avisou timestamps não monotônicos em
uma passagem; logs APU também registram gaps, ainda a investigar.

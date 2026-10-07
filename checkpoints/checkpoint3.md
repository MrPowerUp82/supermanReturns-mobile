# Checkpoint 3: factory corrigida, cena 3D ainda em investigação

Data: 2026-10-07. Continuação de `checkpoint2.md`.

## Correção confirmada

`native/game/main.cpp` instancia explicitamente
`rex::graphics::vulkan::VulkanGraphicsSystem`, do SDK Android completo.
`CMakeLists.txt` deixou de incluir `native_graphics_system_vulkan.cpp` do PC.
Esse arquivo é um placeholder: SetupPresentation/SetupGuestGpu retornam sucesso
sem implementar GPU, presenter, ring buffer ou interrupções. Além do cast inválido
descrito no checkpoint 2, ele não poderia renderizar o jogo mesmo com um patch
para pular InitializeShaderStorage. A factory atual satisfaz o contrato concreto
de ReXApp, sem dummy nem alteração no SDK para esconder o acesso inválido.

Build/link nativo, assemble, lint, ELF ARM64/16 KB, assinatura e zipalign passaram.
O APK foi instalado no S22 preservando os arquivos do jogo. Nos retestes,
InitializeShaderStorage terminou e a thread guest iniciou com status 00000000.
Loading, título, intro em vídeo, HUD, minimapa, áudio e menu de pausa apareceram.
START e A chegaram ao XAM e o menu respondeu. **A cena 3D ainda fica preta.**

## Experimentos

- Perfil herdado: resolução 1x, texturas soft/hard 256/512 MB, trim completo
  a cada 60 frames, altura de render targets limitada a 720.
- PSS atingiu aproximadamente 3,3 GB, Graphics aproximadamente 2,7 GB.
- `vulkan_spirv_optimize=true` e duas threads de criação de pipelines passaram
  pelo boot e chegaram ao HUD. PSS chegou a 3,4 GB. Não corrigiram a cena preta;
  ainda não existe comprovação de benefício de desempenho/memória.
- Teste temporário por TOML com `occlusion_query_enable=false` também continuou
  preto. O resumo GPU confirmou milhares de draws, resolves e frames sem
  placeholders (`skip_present=false`). Não concluir que faltam draws guest ou
  que se trata apenas da compilação assíncrona.
- Próximo reteste remove o limite artificial de altura dos render targets,
  mantendo o restante do perfil. O limite pode cortar áreas intermediárias de
  iluminação, independentemente da resolução final de 720p. O reteste também
  permaneceu preto, inclusive em frame com 3.811 draws e zero placeholders.
  O argumento que impunha 720 foi removido; o limite opcional continua no SDK.
- Encontrado outro cache que retém o pico: VulkanSharedMemory só chamava a
  limpeza da classe base, sem liberar o pool de staging usado para upload.
  `mobile-upload-cache-trim.patch` implementa ClearCache e libera esse pool no
  caminho existente, após AwaitAllQueueOperationsCompletion. Não toca no buffer
  com os dados guest. Build nativa e APK passaram. O aparelho executou trims
  repetidos sem crash. PSS em título/loading ficou por volta de 3,0–3,1 GB;
  não atribuir toda a diferença ao patch, pois a cena e o readback mudaram.
- O último reteste usou `readback_resolve = "full"` temporariamente para
  verificar se a cena depende de dados renderizados acessíveis pela CPU.
  Full sem filtro ficou muito lento; pelo console, o teste foi limitado a
  `vulkan_readback_resolve_max_length 65536`. A cena continuou sem imagem,
  inclusive no frame 811 com 3.784 draws, 26 resolves, zero placeholders e
  skip_present=false. PSS chegou a 3,32 GB; Graphics a 2,75 GB. Não houve crash
  nativo nesse reteste de aproximadamente seis minutos. Isso não comprova
  estabilidade prolongada nem corrige a cena. O novo trim não eliminou o pico.

## Evidências e diagnóstico

`docs/evidence/s22-game-pause.png`: menu de pausa real.
`docs/evidence/s22-game-intro.png`: vídeo de intro real.
`docs/evidence/s22-game-hud-black.png`: HUD/minimapa sobre cena preta.
O arquivo antigo `s22-game-gameplay.png` mostra apenas loading; a afirmação de
gameplay funcional em `docs/validation.md` foi corrigida.

Logs privados, sem commits: `.tools/checkpoint2-boot-logcat.txt`,
`.tools/checkpoint2-optimized-logcat.txt`, `.tools/checkpoint2-visibility-logcat.txt`.
O tag do SDK continua `skate3`; o observador do Swap usa `SupermanGuest`.
Filtrar logcat pelo PID do processo `org.supermanreturns.mobile:game`.

ADB: `C:\Users\webpa\Downloads\scrcpy-win64-v4.0\adb.exe`.
Iniciar LauncherActivity, aguardar carregar, swipe vertical para mostrar o botão,
aguardar terminar a animação, tap em 540,1470. GameActivity não é exportada.
START em 1310,324; A em 2012,777. Usar swipe estacionário de 400 ms para não
perder um toque entre amostragens do guest. Não fazer swipe e tap imediatamente:
o tap pode acontecer durante a animação da lista e não abrir o jogo.

Config temporária do reteste em `files/superman_returns.toml` foi removida ao
terminar, assim como os três TOML temporários em /data/local/tmp. O processo do
jogo foi encerrado e o launcher reaberto. Readback e logging extra não ficaram
habilitados para o usuário. Os argumentos de Activity prevalecem sobre o TOML.
Não alterar/apagar os AST para testar áudio.

Console do SDK funciona via ADB: `input keyevent 68` (grave). O foco abre o
teclado; `input text 'nome_da_cvar%svalor'`, depois `input keyevent 66` envia o
comando. `input keyevent 4` esconde o teclado; `input keyevent 111` tira o foco
da entrada e então `input keyevent 68` fecha o console. Conferir o feedback
`[console] nome = valor` no screenshot. Durante readback, usar holds de 1500–2000
ms nos botões: 400 ms pode ser perdido pela lógica do jogo apesar de existir
o edge no log XAM. O overlay deixou um fundo cinza ao fechar nesta sessão;
investigar separadamente, sem confundir com a imagem 3D originalmente preta.

`tools/prepare_runtime_guest.py` agora só regrava init adaptado/skip_intro quando
o conteúdo muda, evitando recompilar esses arquivos a cada iteração de build.
O original PC permanece intacto nesta continuação. Alterações preexistentes em
docs/superpowers e no projeto PC não foram revertidas.

## Build entregue e próximo problema

APK instalado: `artifacts/superman-returns-mobile-0.1.0-dev.apk`.
SHA-256: `EE4BFC7D1DB57C9F9D80E5E4D40E0A377F4EADD1D23D3D2B6007448A939DE63E`.
Mantém SPIR-V otimizado e duas threads de criação; não prometer melhoria desses
flags, pois os testes não demonstraram ganho de memória ou correção visual.

O cast/crash do checkpoint 2 está resolvido. O bloqueio restante é a imagem 3D
do caminho host/FBO Vulkan no Adreno. Investigar passes de render target,
transferências/resolve e shaders do frame; não substituir por outro placeholder
nem declarar que HUD/menu equivale a jogo jogável. Próxima comparação útil:
captura de um frame e inspeção dos intermediários antes do pós-processamento.
O SDK não usa VkPipelineCache no vkCreateGraphicsPipelines: investigar cache de
driver persistente para reduzir recompilações depois que a imagem estiver certa.

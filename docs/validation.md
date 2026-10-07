# Validação local — 2026-10-07

## Resultado

- `tools/build.ps1`: `assembleDebug` e `lintDebug` aprovados.
- Android lint: **0 erros, 4 avisos** (orientação fixa do diagnóstico, ausência de
  x86 para ChromeOS, singleton com Application Context e texto de diagnóstico
  ainda fora de recursos de localização).
- `tools/test.ps1 -GameRoot ../superman_returns_recomp/game`: **15 testes aprovados**.
- `tools/check_guest_arm64.ps1`: todo o conjunto C++ gerado compilado em archive
  AArch64 com NDK r27, C++23, PCH e quatro jobs; arquivo local de 435.149.912 bytes.
- APK: Android API mínima 33, alvo 35, ABI `arm64-v8a`.
- `tools/verify_apk.py`: ELF64 AArch64, segmentos LOAD com alinhamento de pelo
  menos 16 KB, bibliotecas permitidas e nenhum arquivo ISO/XEX/AST no pacote.
- `apksigner verify` e `zipalign -c -P 16 4`: aprovados.
- Projeto PC consultado permaneceu sem alterações.

APK final desta etapa: `artifacts/superman-returns-mobile-0.1.0-dev.apk`.
SHA-256: `6f7782a8f34ac367683ece612ba2af8a4a68ea37f0be1778904b2b24448e6609`.
Tamanho: 2.872.080 bytes. Assinatura debug local.

## Validação no Galaxy S22

SM-S901E, Android 16 (API 36), SoC SM8450, GPU Adreno 730. Conexão pelo ADB
fornecido com scrcpy v4.0. APK instalado e launcher aberto com sucesso.
O probe anunciou Vulkan 1.1 e suporte a swapchain.

O harness `DeviceSmoke` passou em **11 verificações**:

1. View de controles anexada.
2. Contador de quadros Vulkan avança na tela.
3. Analógico touch altera o estado nativo.
4. Dois pointers mantêm analógico e A pressionados simultaneamente.
5. Soltar o analógico mantém A pressionado.
6. Soltar todos os toques zera o estado.
7. Gatilho esquerdo chega ao estado JNI.
8. Cancelar o gesto limpa o gatilho.
9. Surface Vulkan funciona após rotação para paisagem invertida.
10. Surface Vulkan volta à paisagem original.
11. Fechar e reabrir o diagnóstico apresenta quadros com entrada neutra.

Os testes descobriram e motivaram três correções: habilitar as extensões de
surface no probe; respeitar insets do Android 16; impedir que a Activity antiga
feche a sessão Vulkan pertencente à Activity nova. O último problema foi
reproduzido no primeiro teste e não apareceu após a correção por owner ID.

Evidências: [apresentação Vulkan](evidence/s22-vulkan.png) e
[multitouch simultâneo](evidence/s22-multitouch.png).

## Cobertura do importador

ISO XDVDFS sintética em duas posições de partição; normalização dos nomes para
filesystem Android; extração e progresso; volume truncado; ciclos de diretório;
path traversal; arquivo fora da imagem; nó fora do diretório; AST ausente;
duplicata de nome canônico; cancelamento; importação inválida preservando a
instalação; recuperação de promoção interrompida; SHA-256 conhecido;
validação da cópia local real; promoção bem-sucedida usando hardlinks temporários
para os arquivos reais, sem modificá-los.

## Limites

Ainda **não foram verificados em execução**: importação de ISO pelo SAF,
controle físico real, suspensão prolongada e retomada, consumo térmico e
desempenho de gameplay. A rotação e a reabertura foram verificadas; isso não
constitui teste prolongado de estabilidade.

O archive do jogo é uma verificação de compilação. Ele não está no APK, não
resolve os símbolos do runtime e não prova correção da tradução PowerPC ou
execução do jogo. Nenhuma medição de FPS de gameplay foi feita.

## Runtime experimental no S22

O alvo `native/game/` compilou as 145 unidades geradas e linkou
`libsuperman_game.so` + `librexruntime.so` com `--no-undefined`.
O APK com runtime passou por assemble, lint, assinatura, zipalign e verificação
ELF ARM64 com páginas de 16 KB. Os 15 testes do importador continuam passando.

Os 13 arquivos da cópia local foram transferidos para a pasta privada do app;
o SHA-256 do XEX no aparelho corresponde ao perfil. Isso não testa o fluxo SAF.

Observado em execução: XEX carregado, threads guest, logos EA/DC/WB, tela legal,
loading, frames reais enviados pelo guest, mix de áudio não silencioso e eventos
de botões touch chegando ao XAM/XInput. Evidências: [boot](evidence/s22-game-boot.png)
e [loading](evidence/s22-game-loading.png). Ainda não é uma aprovação de gameplay.

O primeiro boot caiu com `Scudo invalid chunk state` em `PosixThread::set_name`.
Uma corrida entre a atribuição inicial e a renomeação guest foi sincronizada;
os boots seguintes passaram desse ponto. O patch fica em
`tools/runtime-patches/thread-name-race.patch` e é aplicado pelos scripts.

Em 720p, o Android encerrou um teste no loading com razão `LOW_MEMORY`.
O perfil seguinte limita cache de texturas a 256 MB soft / 512 MB hard,
render-to-texture 64 MB e vida soft de 5 segundos. A cena e a estabilidade
desse perfil ainda estão sob teste. Não foi prometido FPS de gameplay.

Um perfil experimental limitou a altura dos render targets a 720. A imagem
[evidência adicional de loading](evidence/s22-game-gameplay.png) mostra somente
a tela de carregamento; não comprova gameplay nem estabilidade de memória.

### Reteste após checkpoint 2

A factory foi ligada ao `VulkanGraphicsSystem` completo do SDK, removendo do
alvo o placeholder Vulkan do PC. Build nativa, assemble, lint, assinatura,
zipalign e verificação ELF passaram. No S22, `InitializeShaderStorage` terminou
e o guest foi iniciado com status `00000000`, superando o crash descrito no
checkpoint. O [menu de pausa](evidence/s22-game-pause.png) apareceu e respondeu
a START/A. A cena permaneceu preta. PSS observado: aproximadamente 3,3 GB;
Graphics: aproximadamente 2,7 GB. Esse teste não comprova gameplay funcional.

Os testes com otimização SPIR-V, occlusion queries desativadas e altura normal
dos render targets também não corrigiram a cena. O limite artificial de 720
foi retirado do perfil. Resumos da GPU confirmaram milhares de draws e resolves,
incluindo frames sem placeholders assíncronos, com a imagem ainda preta.

O patch `mobile-upload-cache-trim.patch` libera o pool temporário de uploads
da memória compartilhada após a sincronização da GPU. O aparelho executou
limpezas repetidas sem crash. O reteste com readback de resolves pequenos
também não corrigiu a cena; PSS chegou a 3,32 GB. A configuração de diagnóstico
foi removida do aparelho ao terminar. Detalhes em `checkpoints/checkpoint3.md`.


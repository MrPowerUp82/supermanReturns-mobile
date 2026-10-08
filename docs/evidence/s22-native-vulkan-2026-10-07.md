# Boot do renderer nativo — Galaxy S22, 2026-10-07

Resultado: **integração nativa executada; gameplay reprovada no boot**.

## Artefatos e ambiente

- PC original: `ae5107814a5686de5fa1eff11a1120d07fa7d883`, checkout preservado.
- Snapshot privado: 200 arquivos, 8 patches; preparação e `--verify` aprovados.
- SHA-256 de `source-lock.json`: `a96f7f69841d4b62e58d49e6ab82126bab40ab730fd675be0ddec5532211b5c7`.
- APK: `artifacts/superman-returns-native-vulkan-0.1.0-dev.apk`.
- SHA-256 do APK: `84faa943411687b96a0960429a4dc28b19470ea6c23c5213fe5ac47107b36f70`.
- Biblioteca privada: 241 shaders prontos, zero falhas de compilação.
- SHA-256 da biblioteca: `d44118ceeac4bb1837667cb49d6841130b3a0efacf52944be873b0c96d2147d6`.
- SM-S901E, Android 16, Adreno 730, Vulkan 1.1; driver `2150002786`.
- Aplicativo de teste: `org.supermanreturns.mobile.native`, label Superman Returns Nativo.

O APK anterior usa outra chave, ausente neste host. A instalação separada preserva
o pacote original e seus dados. Os 13 arquivos locais do jogo foram copiados para
o armazenamento privado do novo pacote. O XEX no aparelho corresponde ao SHA-256
do perfil (`c8f243acd99de9a91f5ae4f409721c0e954e3d5eb96861419d3da07b8106db2b`).
Esse procedimento não valida a importação via SAF. Nenhum AST foi alterado.

## Verificação

- Runtime e biblioteca do jogo ARM64 linkados com `--no-undefined`.
- Assemble, lint, assinatura, zipalign e segmentos ELF de pelo menos 16 KB: PASS.
- Importador: 15 testes PASS.
- Preparação de fontes: 11 testes PASS, incluindo integração com a árvore PC real.
- Coletor: 5 testes PASS; processo parado retorna erro e não aprova gameplay.
- Oito executáveis de componentes, em sete suítes, executados no S22 com o runtime
  recém-compilado: PASS, incluindo 88 verificações Vulkan.
- Revisão independente identificou a corrida de pausa e erros de PM4 que não
  chegavam ao diagnóstico comum. Ambas receberam testes RED/GREEN no S22.

## Observado no boot real

O banner confirma `renderer=pc-native-vulkan`, revisão, digest e hash da biblioteca.
O provider criou swapchain de 2340×1080 com cinco imagens, carregou os 241 shaders
e iniciou a thread Native GPU Commands. O primeiro relatório marcou
`frame=1 hooks=259 packets=32 draws=9 resolves=15 swaps=0 errors=0`.

A execução terminou no pacote 34, um draw, com:

```text
Texture filtering: Guest sampler requests linear filtering of an unsupported format:
format=100 slot=0 dimension=0 extent=64x64 features=118151 fetch3=19404497
```

O formato 100 é `VK_FORMAT_R32_SFLOAT`. O fetch solicita min/mag linear,
mip filter 2 e anisotropia 0. O driver não anuncia
`VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT` para esse formato.
O erro chegou ao launcher; o processo terminou com native crash, status 6.
Os logs privados estão em `.tools/native-filter-game.log` e
`.tools/native-filter-exit-info.txt`; o log de arquivo contém boots anteriores,
portanto cada banner precisa ser associado à execução correta. O coletor usa
logcat restrito ao PID atual para evitar atribuir a ele um digest antigo.

Não foram validados cidade, movimento, voo, combate, áudio contínuo, pausa/retomada,
comparação visual com PC ou sessão de 600 segundos. FPS e memória de gameplay
não foram medidos. A imagem do launcher e a contagem de draws não aprovam esses
critérios. O APK permanece um artefato de diagnóstico.

## Continuação

Revisar a extensão proposta em
`docs/superpowers/specs/2026-10-07-native-float-filter-design.md` antes de mudar o
contrato dos shaders. A precisão float32 e a filtragem solicitada devem ser
preservadas. A integração continua usando o core Vulkan nativo do PC.

Atualização posterior: a extensão foi aprovada e implementada; ver
[filtro float32 v2](s22-native-float-filter-2026-10-07.md). O boot descrito acima
continua sendo o último boot observado, com biblioteca v1.

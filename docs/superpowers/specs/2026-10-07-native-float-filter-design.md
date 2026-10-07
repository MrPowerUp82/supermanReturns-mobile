# Filtragem de texturas float32 no Vulkan nativo Android

Status: **aprovada pelo usuário em 2026-10-07; implementação pendente da revisão do plano**.

## Objetivo e causa comprovada

Continuar a integração aprovada do renderer Vulkan nativo do PC no S22,
preservando imagem, precisão dos recursos e efeitos. O primeiro boot real para
no pacote 34: textura 2D R32_SFLOAT, 64×64, min/mag linear, sem anisotropia. O
driver Adreno 730 disponível não suporta filtragem linear desse formato.

O núcleo PC pressupõe esse suporte ao vincular o sampler. A correção precisa
atuar no core e na biblioteca de shaders em conjunto. O código PC original,
ASTs, layout guest 1280×720 e critérios de validação permanecem preservados.

## Abordagem proposta

Manter as texturas em float32. Para texturas 2D cujo formato não oferece filtro
linear, calcular a filtragem no helper HLSL usando amostras pontuais e interpolação
float32. O sampler pontual continua aplicando endereçamento; a interpolação
reproduz minificação/magnificação e seleção/interpolação de mip conforme o estado
capturado. Aplicar conversão de gamma depois da filtragem, como no caminho atual.

Usar o caminho existente de hardware quando o formato suporta os filtros pedidos.
Não desabilitar a verificação existente antes de o caminho por shader estar pronto.
Estados ainda não implementados, incluindo anisotropia ou dimensões adicionais
sem equivalência demonstrada, continuam produzindo erro explícito com contexto.
Encontrá-los exige ampliar a implementação e seus testes antes de prosseguir.

O estado do sampler e a seleção do helper precisam chegar ao shader sem mudar
os índices reais das arrays. Definir uma revisão nova do contrato de bindings e
do artefato de shader, com bits reservados documentados e máscaras explícitas.
O loader deve rejeitar bibliotecas da revisão anterior quando o core exigir a
nova semântica; atualizar somente o renderer ou somente a biblioteca não pode
produzir uma imagem silenciosamente incorreta. Incluir a revisão no cache.

Preparar uma cópia privada do common HLSL e das ferramentas PC necessárias,
aplicando adaptações versionadas no projeto mobile. Registrar os hashes das
entradas e saídas. O emissor e DXC continuam rodando no host; a biblioteca
compilada continua privada no aparelho. Nenhum arquivo do checkout PC é editado.

## Fronteiras de implementação

- `tools/native-patches/`: recursos, planejamento de sampler, bindings,
  descriptors, contrato e loader na árvore PC privada.
- Preparação de shaders: common HLSL adaptado, ferramentas de serialização e
  verificação com a nova revisão; metadados de proveniência.
- Testes: contrato incompatível, estado de filtro, precisão numérica e execução
  Vulkan real da filtragem float32 no S22.
- Bootstrap: exigir biblioteca compatível antes de iniciar o guest.

Esta extensão não cria outro renderer, não troca o backend, não converte texturas
para float16 e não retira draws ou efeitos. Pode acrescentar custo de GPU;
desempenho será medido depois de confirmar a imagem.

## Critérios antes de ativar o caminho

1. Teste RED reproduz a ausência de filtragem linear de R32_SFLOAT no S22.
2. Testes de planejamento cobrem os filtros efetivamente solicitados, estados
   mistos min/mag, mipmaps, bordas, repeat e mirror. Estados não cobertos falham.
3. Um teste Vulkan com readback usa valores float32 que revelem perda por float16,
   amostra centros, meias posições e bordas, e compara com referência numérica
   independente com tolerância explícita. Testar mip e min/mag separadamente.
4. Bibliotecas antigas/incompatíveis são rejeitadas antes de Resume. Os testes
   existentes de shaders, recursos, descriptors e bootstrap continuam passando.
5. Gerar e verificar novamente a biblioteca inteira; nenhum shader utilizado
   pode faltar ou ser ignorado. Repetir o boot e localizar a primeira divergência.
6. Concluir os critérios originais: referência visual PC, cidade, movimento,
   câmera, voo, pouso, combate, áudio, pausa/fundo/retorno e 600 segundos.

Após aprovação desta proposta, detalhar a execução e os pontos exatos do contrato
no plano de implementação. A aprovação do diagnóstico atual não equivale a PASS
de gameplay.

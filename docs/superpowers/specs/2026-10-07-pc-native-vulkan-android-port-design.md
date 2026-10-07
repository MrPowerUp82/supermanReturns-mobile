# Port do renderer nativo Vulkan do PC para Android

Data: 2026-10-07
Estado: desenho e especificação escrita aprovados pelo usuário em 2026-10-07.

## Objetivo e entendimento aprovado

O usuário quer executar Superman Returns no Galaxy S22 Snapdragon usando o
renderer nativo Vulkan do projeto PC `superman_returns_recomp`. Corrigir apenas
o backend Vulkan Xenos do SDK não atende ao pedido. O usuário escolheu
explicitamente portar o renderer do PC e aprovou o desenho apresentado.

O primeiro marco é uma integração funcional completa: cidade e personagem
visíveis, movimento, voo, combate, áudio e controles no aparelho. A prioridade
assumida e apresentada foi imagem correta e estabilidade antes de otimizar FPS.
30 FPS é a meta de desempenho, não um resultado garantido por esta integração.
O marco funcional não equivale a certificar todas as fases ou paridade completa.

## Evidência e ponto de partida

- Destino: SM-S901E, Snapdragon 8 Gen 1, Adreno 730, Android 16, ARM64.
- O APK atual instancia `rex::graphics::vulkan::VulkanGraphicsSystem` em
  `native/game/main.cpp`. Inicia o guest, mostra menus, vídeo e HUD, mas a cena
  3D permanece preta. O consumo observado chegou a aproximadamente 3,3 GB PSS.
- No PC, o caminho nativo captura comandos e recursos e entrega pacotes ao
  renderer Vulkan próprio, em `port/src/graphics/vulkan`. A seleção nativa usa
  `sr_renderer=native` e `sr_native_api=vulkan`.
- `native_graphics_system_vulkan.cpp/.h` do PC contém funções vazias: não oferece
  presenter, ring buffer, interrupções ou progresso. Não é uma implementação
  utilizável por simplesmente trocar a factory.
- `android_provider.cpp` existe, mas ainda inclui `rex/ui/surface_win.h`.
  Sua existência não prova compilação nem conexão com os hooks do jogo.
- `NativeFrontend` já separa parte do estado, mas `Renderer` ainda mistura
  captura e responsabilidades D3D12. Reutilizar apenas esse struct é insuficiente.
- `ReXApp` e funções de vídeo do SDK fazem `static_cast<GraphicsSystem*>` de
  `IGraphicsSystem`. O renderer próprio implementa outra hierarquia. A correção
  precisa remover essa incompatibilidade, sem fingir que o objeto é Xenos.
- O processo Android de shaders atual sempre retorna erro. O PC dispõe da
  biblioteca `superman_returns_vulkan.srvk`; arquivos `.exe` não podem ser
  utilizados como compiladores no aparelho.

Os documentos anteriores sobre backend Android e desacoplamento são histórico.
Este documento define o escopo completo da integração funcional e não altera
aqueles arquivos nem presume que suas tarefas foram concluídas.

## Decisão de arquitetura

Reutilizar o núcleo nativo Vulkan e a lógica de captura do PC. Introduzir as
adaptações de plataforma necessárias e conectar o guest a esse caminho completo.
Não substituir o núcleo por um renderer demonstrativo nem encaminhar seus draws
ao backend Vulkan Xenos do SDK.

Fluxo de um quadro:

`guest ARM64 recompilado -> hooks XDK e captura nativa -> pacotes com recursos
próprios -> GameFrame Vulkan do PC -> composição -> presenter Android -> tela`

O processamento de comandos, MMIO, interrupções, writeback e sincronização que
o guest observa acompanha esse fluxo e deve preservar as semânticas do PC.

### 1. Fontes compartilhadas e fronteira D3D12

Usar o projeto PC local como fonte identificada dos componentes reutilizados.
Registrar o commit e as diferenças locais dos arquivos consumidos para tornar
a build reproduzível; não depender de alterações invisíveis na pasta vizinha.
Uma etapa de preparação copia os arquivos selecionados para a árvore privada
de build, aplicando patches versionados neste repositório quando necessário.

Extrair as operações de captura, e não somente seus campos, para uma unidade
independente de D3D12. Ela produz os mesmos pacotes, snapshots e identificadores
de recursos utilizados pelo Vulkan do PC. COM, DXGI, HWND e execução de processos
Windows ficam fora do conjunto de fontes Android.

Substituir leituras `ReadProcessMemory` por acesso validado aos mapeamentos do
guest fornecidos pelo SDK Android. Verificar limites e aliases físicos antes de
copiar; falhas devem identificar endereço, tamanho e operação. O worker Vulkan
recebe snapshots possuídos pelo pacote e não lê memória guest mutável.

### 2. Contrato com o runtime e execução do guest

Aplicar um patch reproduzível ao fork Android fixado para expor pelo contrato
gráfico os acessos necessários de apresentação, inicialização de shaders e vídeo.
Atualizar seus consumidores para chamadas virtuais válidas, removendo os casts
que pressupõem uma instância de `rex::graphics::GraphicsSystem`.

Os backends existentes conservam suas implementações. O sistema nativo fornece
provider, presenter, configuração de ring buffer, callback de interrupção,
writeback, identificador de progresso, contagem de quadros e shutdown reais.
Auditar também pausa, retomada, cache, tracing e recuperação de device loss:
funções sem suporte devem produzir erro explícito ou indisponibilidade declarada,
sem casts inválidos ou retornos de sucesso fictícios.

Reaproveitar o processador de comandos nativo do PC, isolando sua criação de
provider das dependências Windows. Preservar ordenação, MMIO, eventos, waits,
vblank e progresso; uma thread adormecida precisa poder ser acordada no shutdown.
Os hooks XDK de desenho, recursos, estados, resolves e swap do PC devem ser
instalados com os endereços e assinaturas da mesma imagem guest validada.
O observador de swap atual não substitui esses hooks.

### 3. Provider e ciclo de vida Android

Adaptar o provider existente para `ANativeWindow` e
`VK_KHR_android_surface`, mantendo `GameFrame`, recursos, resolves, composição e
gerenciamento de fences do núcleo PC. Compartilhar código de plataforma neutro
sem uma refatoração geral do projeto PC.

Só iniciar o guest após provider, presenter, shader library e conexão nativa
estarem prontos. Publicar explicitamente a identidade do renderer no log.
Perda/recriação da superfície, ida ao fundo e retorno devem suspender submissões
incompatíveis com a superfície e reconstruir a apresentação com sincronização.
Não destruir imagens ou buffers ainda referenciados por submissões pendentes.
Device loss deve encerrar a execução com diagnóstico, sem continuar desenhando
com recursos inválidos.

### 4. Shaders Vulkan

Gerar e validar no PC a biblioteca `.srvk` usando os dados locais do usuário e o
tradutor empregado pelo renderer Vulkan PC. Instalar a biblioteca no diretório
privado do aplicativo por ferramenta reproduzível. A biblioteca contém dados
derivados do jogo: não será commitada nem incorporada ao APK público.

Carregar a biblioteca antes do guest e conferir formato, contagem e compatibilidade
com o consumidor. Inicializar o armazenamento nativo sem chamar o cache Xenos.
Cache de pipelines do driver fica separado, privado e identificado por GPU/driver
e versão do conteúdo, com invalidação de arquivos incompatíveis.

Nesta integração, tradução de shaders desconhecidos acontece no PC. Quando um
shader necessário não estiver na biblioteca, capturar container, estágio e hash
para regeneração e interromper com diagnóstico claro. Não omitir desenhos
silenciosamente nem reportar gameplay correta com shaders ausentes.
Um compilador de shaders executado dentro do Android é um projeto posterior.

### 5. APK, controles e memória

Conectar a factory nativa apenas depois de cumprir os contratos acima. A build
inclui o frontend nativo, hooks, core Vulkan e provider Android completos; o log
e os contadores devem comprovar a passagem dos draws por eles. Falha ao iniciar
o nativo impede iniciar o jogo com mensagem útil; não existe fallback automático
para o renderer Xenos.

Preservar a importação dos 13 arquivos do jogo, áudio/XMA, saves e ponte de input.
Manter inicialmente o layout interno 1280x720 do PC; não alterar apenas o tamanho
de apresentação para reduzir memória, pois isso pode quebrar pitches de resolves.
Controles touch precisam permitir movimento e câmera simultâneos, voo, ataque,
pausa e navegação; perda de foco solta todos os botões. Validar controle físico
se houver um conectado, sem exigir que o usuário adquira um.

Reutilizar recursos e liberar temporários após fences. Medir PSS, memória gráfica,
picos e estabilidade. Não presumir que o port será leve apenas por ser nativo.
O trimming do SDK atual não é automaticamente uma política válida para o core PC.

## Validação e critérios de aceite

1. Build ARM64 reproduzível, link sem símbolos ausentes, APK assinado, alinhamento
   de 16 KB e lint Android. Registrar revisões, comandos e hashes dos artefatos.
2. Testes de contrato: apresentação e inicialização de shaders sem cast inválido;
   ring buffer, interrupção, writeback, progresso e shutdown exercitados. Testar
   os limites de memória guest e o diagnóstico de shader ausente.
3. Confirmar no S22, por logs e contadores, a factory nativa, os hooks, pacotes,
   draws e resolves processados pelo core Vulkan do PC. Um swap isolado não basta.
4. Capturar uma cena equivalente no PC com Vulkan nativo e no S22. Comparar
   personagem, cenário, texturas, iluminação, profundidade, pós-processamento e
   HUD. Usar intermediários do renderer para localizar diferenças; conservar
   também a evidência das limitações já existentes no PC.
5. No aparelho, iniciar jogo novo e executar movimento, câmera, voo e combate com
   resposta aos controles e áudio. Registrar screenshots e roteiro de ações.
6. Executar uma sessão de pelo menos 10 minutos na área inicial, incluindo pausa
   e retomada após ir ao fundo. Não pode haver crash, encerramento por memória,
   imagem permanentemente preta ou input preso. Registrar memória a cada minuto
   e investigar crescimento contínuo sob repetição da mesma rota antes de aceitar.
7. Medir FPS e tempos de quadro no mesmo roteiro, distinguindo primeira execução
   e cache aquecido. Reportar resultados reais; abaixo da meta de 30 FPS, declarar
   explicitamente a diferença e o gargalo, sem chamar isso de paridade de desempenho.
8. A build PC dos componentes compartilhados deve continuar compilando. Executar
   as suítes existentes pertinentes e repetir a cena de referência se a lógica
   compartilhada mudar. Não sobrescrever alterações preexistentes do usuário.

## Limites e entrega

Esta etapa entrega a integração nativa e sua validação funcional na área inicial.
Não certifica campanha inteira, War World, todas as variantes de shader ou
qualidade superior ao PC. Correções necessárias para cumprir o roteiro pertencem
ao escopo; otimizações gerais e novos recursos gráficos ficam para depois.

Entregar código e patches reproduzíveis, ferramentas de preparação de fontes e
shaders locais, APK de teste, instruções de build e relatório com evidências,
memória, FPS e limitações. Não modificar ou apagar ASTs para contornar áudio.
O artefato atual pode ser mantido para diagnóstico, mas não será apresentado
como a versão nativa prometida.

## Próxima etapa do processo

Após a revisão desta especificação pelo usuário, produzir o plano de implementação
com dependências, arquivos, testes e marcos executáveis. A aprovação conversacional
recebida autoriza esta especificação; não substitui a revisão deste documento nem
a aprovação do plano e a escolha do método de execução exigidas pela skill.

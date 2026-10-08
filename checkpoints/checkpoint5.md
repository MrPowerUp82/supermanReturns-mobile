# Checkpoint 5 — integração nativa e incompatibilidade de filtro

**Continuação mais recente:** [checkpoint6](checkpoint6.md). Spec e plano do filtro já aprovados e implementados; boot v2 pendente de reconexão ADB.

Data: 2026-10-07. Workspace:
`C:/Users/Gusta/Documents/outros-projetos/supermanReturns-mobile`.
Branch `codex/resume-native-vulkan`, base `43d9373`; alterações ainda sem commit.

## Intenção e autorização

Continuar checkpoint4. O usuário forneceu o PC em
`C:/Users/Gusta/Documents/outros-projetos/superman_returns_recomp` e scrcpy em
`C:/Users/Gusta/Downloads/scrcpy-win64-v5.0`. Renderer deve ser nativo e Vulkan.
Plano principal aprovado:
`docs/superpowers/plans/2026-10-07-pc-native-vulkan-android-port.md`.
Nenhum fallback para Xenos, redução silenciosa de efeitos ou alteração de AST.
Checkout PC original preservado, revisão `ae5107814a5686de5fa1eff11a1120d07fa7d883`.

## Realizado nesta retomada

- Restaurados SDK35, NDK27.2.12479018, JDK17 e runtime fixado
  `edd4344723ecac3ffa18c5dcd2fcc268f468ff9e` com submódulos.
- Restauradas criações de provider/interface/frontend que o PC real não tinha
  versionadas. Elas agora são patches mobile reproduzíveis, sem editar o PC.
- Capturadas as alterações inacabadas do bootstrap em `zz-native-bootstrap.patch`.
  Corrigidos artefato de Markdown, definição duplicada de gamma e include xxHash.
- Factory, hooks, bridge, falhas visíveis no launcher e pausa JNI conectados.
- Revisão independente `/root/review_native_integration`: duas falhas Important
  corrigidas, com testes RED/GREEN no S22, em `zzz-command-failure.patch`:
  pausa não encerra worker; PM4 inválido/exceção chegam ao handler de falha comum.
- Full build/link ARM64 e oito executáveis de componentes aprovados com o
  runtime recém-compilado. Testes: 15 importador, 11 fontes, 5 coletor; 88 checks
  na suíte Vulkan. Todos PASS nos comandos registrados.
- Biblioteca privada gerada: 241/241 prontos, zero falhas. APK validado e instalado.
- Coletor novo restringe logcat ao PID, interrompe ao trocar PID e não certifica
  gameplay. Corrigida atribuição de banner antigo: `game.log` é append, portanto
  boot identity vem do logcat restrito ao PID atual.

## Aplicativo de teste e artefatos

Dispositivo `RXCWB05KQMX`, SM-S901E, Android16, Adreno730. A chave do APK
anterior não existe neste host. Mantida a instalação original; o teste usa
`org.supermanreturns.mobile.native` (Superman Returns Nativo), classes Java
`org.supermanreturns.mobile.*`. Build com `tools/build.ps1 -NativeSideBySide`.
Os 13 arquivos (~2 GB) foram copiados à pasta privada do novo pacote; shaders
instalados com `prepare_native_shaders.ps1 -Package org.supermanreturns.mobile.native`.

APK atual e hashes estão no relatório
`docs/evidence/s22-native-vulkan-2026-10-07.md`.
Artifact: `artifacts/superman-returns-native-vulkan-0.1.0-dev.apk`.
Snapshot `.tools/pc-native`, fontes guest `.tools/android-guest`, build
`.tools/game-build`, libs empacotadas `android/app/libs/arm64-v8a`.
Source-lock atual SHA256 `a96f7f69841d4b62e58d49e6ab82126bab40ab730fd675be0ddec5532211b5c7`.
Biblioteca `.tools/native-shaders/superman_returns_vulkan.srvk`.
Logs privados `.tools/native-filter-game.log`, `.tools/native-filter-exit-info.txt`,
`.tools/filter-diagnostic-build.log`, `.tools/filter-diagnostic-apk.log`.

## Bloqueio comprovado

Boot nativo cria swapchain e executa 9 draws/15 resolves. Pacote 34 falha:
`format=100 slot=0 dimension=0 extent=64x64 features=118151 fetch3=19404497`.
R32_SFLOAT (100), min/mag linear, mip2, aniso0; driver não anuncia filtro linear.
Não remover essa checagem ou trocar por nearest/float16 para passar do boot.
`descriptor_sets.cpp` contém a checagem; diagnóstico ampliado versionado em
`zzzz-filter-diagnostic.patch`.

Especificação aprovada pelo usuário na resposta `sim`, ainda não implementada:
`docs/superpowers/specs/2026-10-07-native-float-filter-design.md`.
Plano de execução criado:
`docs/superpowers/plans/2026-10-07-native-float-filter.md`.
Próxima ação: obter revisão desse plano conforme writing-plans, preservando
execução Native nesta conversa; depois implementar filtragem float32 com teste GPU.
Tarefas 7/8 do plano principal continuam incompletas: nenhum PASS funcional,
nenhuma sessão de 600 segundos, sem medição de FPS/memória de gameplay.

## Comandos e cuidados para retomar

CMake moderno disponível em
`C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`.
`cmake --build .tools/game-build --target superman_game --parallel 4`.
Depois copiar libsuperman_game.so e runtime ao diretório de libs e empacotar.
Nunca compilar simultaneamente a mesma árvore.

Aplicar mudanças privadas por patches e preparar novamente, verificando lock.
Repreparar altera timestamps; pode recompilar core e shader helpers.
DXC: PC `.tools/dxc/bin/x64/dxc.exe`. Emitter:
PC `build/vulkan-m2/emitter-build/XenosRecompCorpus.exe`. Common HLSL:
PC `build/vulkan-m2/emitter-tree/src/XenosRecomp/shader_common.h`.
Manter PC intacto; uma adaptação do common exige cópia privada e proveniência.

Instalar com adb install -r; GameActivity não é exportada. Abrir LauncherActivity,
rolar e localizar botão via uiautomator dump do próprio app antes de tocar.
O instalador reseta a posição de scroll. Não assumir coordenadas antigas.
Os logs de arquivo são append: escolher a execução corrente, nunca o primeiro
banner. `$nativeGamePid=([string](& $adb shell pidof ...)).Trim()` evita null.Trim.

Staging de dados ainda presente no aparelho:
`/data/local/tmp/sr-native-20261007-1928/game`; pode ser limpo após verificar
integridade de todos os arquivos copiados. Nada foi removido da instalação original.
Ledger de execução `.superpowers/sdd/2026-10-07-pc-native-vulkan-android-port/progress.md`.

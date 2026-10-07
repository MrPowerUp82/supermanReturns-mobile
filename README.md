# Superman Returns Mobile

Port Android experimental de Superman Returns (Xbox 360), com alvo inicial no
**Galaxy S22 Snapdragon**. Baseado na recompilação local de Superman Returns e
na arquitetura Android do [Skate3-Mobile](https://github.com/Buku313/Skate3-Mobile).

O projeto inclui um alvo experimental de gameplay com o runtime ReXGlue Android,
renderer nativo Vulkan do projeto PC, áudio XMA e controles touch. É preciso compilar esse
alvo antes do APK para incluir o jogo recompilado; sem ele, o APK oferece apenas
instalador e diagnóstico. Boot e gameplay precisam de validação no aparelho.

## O que está implementado

- App Android 13+, ARM64, interface em português, sem permissão de internet.
- Seleção de ISO pelo seletor de arquivos do Android e extração local de XDVDFS.
- Validação SHA-256 do `default.xex` e tamanhos exatos dos 12 arquivos `.AST` exigidos.
- Instalação em pasta temporária: falha ou cancelamento preservam a instalação
  anterior; uma promoção interrompida é recuperada na próxima abertura.
- Núcleo Vulkan reaproveitado do projeto PC: loader, escolha da GPU, device,
  swapchain, sincronização e apresentação em uma Surface Android.
- Diagnóstico que envia comandos Vulkan reais: painel vermelho com brilho que
  muda ao pressionar botões. **Esse painel não é uma cena do jogo.**
- Dois analógicos, gatilhos, D-pad e botões touch simultâneos; leitura de
  controle físico e envio para um estado JNI com o layout de bits de XInput.
  Na build de gameplay, o estado é enviado ao driver XInput SDL do runtime.
- Linkagem e alinhamento do pacote preparados para páginas Android de 16 KB.
- Alvo CMake separado para verificar o C++ gerado do jogo em ARM64, sem incluir
  esse código ou dados comerciais no APK de diagnóstico.
- Alvo `native/game/` para recompilar os mesmos sources com os headers do fork
  Android, linkar o runtime Vulkan/SDL e aplicar a correção XMA do projeto PC.

## Instalar a build local

Após a compilação, o APK fica em
`artifacts/superman-returns-mobile-0.1.0-dev.apk`. Instale no celular e abra
**Testar Vulkan e controles**. O diagnóstico funciona sem importar uma ISO.
Use Voltar/gesto de navegação para sair do diagnóstico.

Para preparar os dados, use **Selecionar minha ISO** com um dump local da sua
própria edição Xbox 360 suportada. Não há download de jogo ou Title Update.
Serão necessários aproximadamente 2,1 GiB livres além da ISO; uma reinstalação
mantém a instalação anterior até validar a nova cópia. A importação continua ao
girar a tela ou trocar de Activity enquanto o processo continuar vivo. O Android
pode encerrar o processo em segundo plano: mantenha o app aberto para concluir.
Não há retomada parcial; arquivos incompletos são removidos na próxima tentativa.

O SHA-256 só autentica o XEX. Os AST são conferidos quanto à presença e tamanho
exato da edição suportada; corrupção interna dos arquivos AST ainda não é detectada.

## Compilar no Windows

Instale JDK 17 e o Android SDK com:

```text
platforms;android-35
build-tools;35.0.0
ndk;27.2.12479018
cmake;3.22.1
```

```powershell
$env:JAVA_HOME = 'C:\caminho\jdk-17'
$env:ANDROID_HOME = 'C:\caminho\android-sdk'
.\tools\build.ps1
.\tools\test.ps1
```

O build gera um APK assinado com a chave **debug local**. Não é uma release
assinada para publicação. O script executa `assembleDebug` e `lintDebug`.
Se a chave do app já instalado não estiver disponível, use
`tools/build.ps1 -NativeSideBySide`: gera `superman-returns-native-vulkan-0.1.0-dev.apk`
como **Superman Returns Nativo**, pacote `org.supermanreturns.mobile.native`.
Esse app usa armazenamento separado e preserva a instalação anterior. Passe
`-Package org.supermanreturns.mobile.native` ao instalador de shaders e ao
coletor `tools/validate_native_device.ps1` ao validar essa build.
As ferramentas baixadas nesta sessão ficam em `.tools/`, ignorada pelo Git.
Também é possível abrir a pasta `android/` no Android Studio.

Para incluir o runtime experimental (também exige Python, Git e CMake 3.25+):

```powershell
.\tools\setup_game_runtime.ps1
.\tools\build_game.ps1 -RecompRoot '..\superman_returns_recomp' -Jobs 4
.\tools\build.ps1
```

Os arquivos gerados são copiados para uma pasta privada e recompilados com os
mesmos headers do runtime. O adaptador remove apenas metadados `codegen_flags`
com todos os campos falsos, ausentes no fork anterior; isso não confirma toda a
compatibilidade em execução. O renderer nativo usa o layout original
1280×720 e uma biblioteca SPIR-V preparada no Windows. Ele não usa fallback
automático para o backend Xenos. Prepare a biblioteca antes de iniciar:

```powershell
.\tools\prepare_native_shaders.ps1 -RecompRoot '..\superman_returns_recomp'
# Após instalar o APK com adb install -r:
.\tools\prepare_native_shaders.ps1 -RecompRoot '..\superman_returns_recomp' -Install -Device '<serial>'
```

Depois de importar os dados, use **Iniciar jogo experimental**.

Para testar em um aparelho autorizado por ADB:

```powershell
.\tools\test_device.ps1 -Serial '<serial de adb devices>' -Adb 'C:\caminho\scrcpy\adb.exe'
```

O harness separado injeta dois toques, confere o estado JNI, testa rotação e
reabertura do diagnóstico e copia screenshots para `artifacts/device-evidence/`.
Não abre arquivos pessoais nem importa uma ISO. Em 2026-10-07, os **11 checks
passaram no Galaxy S22 SM-S901E / Android 16 / Adreno 730**.

## Verificar o código do jogo em ARM64

Com a recompilação de PC e seus arquivos gerados disponíveis:

```powershell
.\tools\test.ps1 -GameRoot '..\superman_returns_recomp\game'
.\tools\check_guest_arm64.ps1 -RecompRoot '..\superman_returns_recomp' -Jobs 4
```

O segundo comando cria um overlay privado dos headers ReXGlue v0.10.0 e
estende seus fallbacks de libc++ para Android (`from_chars` de ponto flutuante
e conversões de relógio). Não modifica o projeto PC. Compila os sources gerados
para um archive local em `.tools/guest-build/`; **não gera um jogo executável**.
Os símbolos do runtime e dos hooks ainda exigem implementações ao linkar.

## Caminho experimental de gameplay

O alvo atual adapta o renderer nativo Vulkan do PC por patches aplicados a uma
cópia privada em `.tools/pc-native`. O bootstrap seleciona esse renderer;
captura, comandos e apresentação Android não delegam draws ao backend Xenos.
`source-lock.json` registra revisão, hashes dos fontes e adaptações. Shaders
usados precisam existir na biblioteca privada `.srvk`; uma falta interrompe
a execução e produz diagnóstico, sem omitir silenciosamente o draw.
A integração ReXGlue fornece memória, threads, filesystem, áudio XMA e XInput.
Os controles touch se conectam ao driver SDL e a correção XMA é recompilada
com os headers do mesmo runtime.

A compatibilidade completa do codegen com o fork anterior ainda depende dos
testes de execução. É preciso validar boot, menus, gameplay, imagem, áudio,
suspensão e consumo de memória no Galaxy S22 antes de prometer FPS.
As imagens de gameplay já existentes em `docs/evidence` pertencem ao backend
anterior. Elas não comprovam a validação visual deste caminho nativo.

O perfil de 30 FPS é um alvo de trabalho, não uma opção funcional do jogo nesta
build. Veja [o plano técnico](docs/android-port.md) e
[a validação](docs/validation.md).

## Referências e procedência

- [Skate3-Mobile](https://github.com/Buku313/Skate3-Mobile), revisão
  `e1b28c185d578c22e3a630f6f3b13ed123462a43`: referência de launcher,
  armazenamento privado, importação de ISO, touch e empacotamento ARM64.
- `../superman_returns_recomp`, revisão
  `ae5107814a5686de5fa1eff11a1120d07fa7d883`: perfil do jogo e os 12 arquivos
  de infraestrutura Vulkan copiados para `native/vulkan/`.
- [Runtime Android usado pelo Skate3-Mobile](https://github.com/Buku313/rexglue-skate3-android),
  revisão `edd4344723ecac3ffa18c5dcd2fcc268f468ff9e`: referência para a migração
  e runtime linkado na build opcional de gameplay.

Nenhum executável comercial, ISO, AST, shader extraído ou código gerado a partir
do jogo é versionado. [Avisos de terceiros](THIRD_PARTY_NOTICES.md).

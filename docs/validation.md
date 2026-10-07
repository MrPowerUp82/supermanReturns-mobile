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
SHA-256: `dbab96f30bdcbdccde259e4d1ed5c1904177d0abb8732a358dedc80e3cd3cb63`.
Tamanho: 1.513.880 bytes. Assinatura debug local.

## Cobertura do importador

ISO XDVDFS sintética em duas posições de partição; normalização dos nomes para
filesystem Android; extração e progresso; volume truncado; ciclos de diretório;
path traversal; arquivo fora da imagem; nó fora do diretório; AST ausente;
duplicata de nome canônico; cancelamento; importação inválida preservando a
instalação; recuperação de promoção interrompida; SHA-256 conhecido;
validação da cópia local real; promoção bem-sucedida usando hardlinks temporários
para os arquivos reais, sem modificá-los.

## Limites

`adb devices` não listou aparelhos. Portanto, **não foram verificados em
execução**: aparência no S22, importação pelo SAF, apresentação Vulkan, touch,
controle físico, suspensão e retomada ou consumo térmico.

O archive do jogo é uma verificação de compilação. Ele não está no APK, não
resolve os símbolos do runtime e não prova correção da tradução PowerPC ou
execução do jogo. Nenhuma medição de FPS de gameplay foi feita.

O APK atual é um instalador/diagnóstico de desenvolvimento. Gameplay, runtime
completo, renderer de cenas e áudio do jogo continuam pendentes.

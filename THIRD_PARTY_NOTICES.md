# Procedência e avisos

`native/vulkan/*` foi copiado do projeto local `superman_returns_recomp`,
revisão `ae5107814a5686de5fa1eff11a1120d07fa7d883` em 2026-10-07.
Os hashes dos arquivos originais estão em `docs/provenance.json`.
Nenhuma licença global de terceiros é presumida para os arquivos próprios
daquele projeto; verificar as condições do projeto antes de redistribuí-lo.

O desenho do instalador Android, os offsets XDVDFS e as decisões de touch/
ARM64 foram estudados no [Skate3-Mobile](https://github.com/Buku313/Skate3-Mobile),
revisão `e1b28c185d578c22e3a630f6f3b13ed123462a43`. As classes do instalador,
launcher e controles são implementações novas. Não foram copiados assets, branding, mods,
shaders de jogo, código gerado ou binários desse repositório.

O Gradle Wrapper em `android/gradlew*` e `android/gradle/wrapper/` foi obtido
da referência Skate3-Mobile. Gradle é distribuído sob
[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0).
O texto está em `third_party/licenses/Apache-2.0.txt`.

O alvo opcional `native/guest/` usa, apenas localmente, headers ReXGlue v0.10.0,
SIMDe, fmt, spdlog e outras dependências já instaladas no projeto PC. Seus
avisos e licenças devem ser preservados caso sejam redistribuídos. Esse alvo
e seus headers não são incluídos no APK atual.

Android SDK/NDK e Microsoft OpenJDK são ferramentas locais de compilação e
não são redistribuídos no repositório. O APK inclui o runtime C++ do NDK,
distribuído conforme os avisos libc++ do NDK em
`third_party/licenses/NDK-libcxx-NOTICE.txt`.

Superman Returns e suas marcas pertencem aos respectivos titulares. Não há
arquivos comerciais do jogo no APK ou nas fontes versionadas.

A opção de gameplay usa o fork Android ReXGlue na revisão
`edd4344723ecac3ffa18c5dcd2fcc268f468ff9e`, com SDL3 e dependências fixadas
pelos submódulos do upstream. As classes `org/libsdl/app` vêm desse SDL3.
Os textos de licença e avisos dos diretórios upstream são reunidos por
`tools/collect_runtime_notices.py` em `assets/runtime-notices.txt` no APK.
ReXGlue usa BSD-3-Clause e SDL usa zlib; as demais dependências preservam
seus próprios termos, incluindo FFmpeg LGPL. O código gerado e a correção
XMA vêm da recompilação local e são preparados em `.tools/android-guest/`.

As modificações locais do runtime ficam em `tools/runtime-patches/`: sincronização
de nomes de threads, limpeza periódica dos caches Vulkan, limite opcional de
altura de render targets e liberação dos buffers temporários de upload após
sincronização da GPU. Os avisos upstream permanecem preservados.

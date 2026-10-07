# Procedência e avisos

`native/vulkan/*` foi copiado do projeto local `superman_returns_recomp`,
revisão `ae5107814a5686de5fa1eff11a1120d07fa7d883` em 2026-10-07.
Os hashes dos arquivos originais estão em `docs/provenance.json`.
Nenhuma licença global de terceiros é presumida para os arquivos próprios
daquele projeto; verificar as condições do projeto antes de redistribuí-lo.

O desenho do instalador Android, os offsets XDVDFS e as decisões de touch/
ARM64 foram estudados no [Skate3-Mobile](https://github.com/Buku313/Skate3-Mobile),
revisão `e1b28c185d578c22e3a630f6f3b13ed123462a43`. As classes Java deste
projeto são implementações novas. Não foram copiados assets, branding, mods,
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

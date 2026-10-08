 Biblioteca v2 e preparação rastreável

**Interfaces:** `prepare_native_shader_tools.py --recomp <PC> [--output <dir>] [--verify]`, saída padrão `.tools/native-shader-tools`, `source-lock.json` com revisão, hashes de entradas, patches, helper e saídas. Expor `LIBRARY_VERSION=2` nas ferramentas privadas e usar `BindingContractVersion` no loader privado. `VulkanShaderConfig` permanece precompiled-only no Android.

- [ ] Escrever `test_v2_tool_snapshot`, `test_verify_detects_helper_change`, `test_incompatible_patch_preserves_previous_output`: verificar versões, hashes, detecção de alteração e staging atômico.
- [ ] Executar `python -m unittest discover -s tests -p test_prepare_native_shader_tools.py`; registrar RED por ferramenta ausente.
- [ ] Implementar preparação copiando `tools/shaders/*.py` e o common existente para árvore privada; aplicar patch versionado. Incorporar o conteúdo completo do helper ao common para que seu hash cubra o código consumido pelo compilador. Recusar fonte/destino sobrepostos e publicar somente após validação.
- [ ] Adaptar as cópias de `vulkan_contract.py`, `runtime_vulkan_shader.py`, `make_vulkan_preshaders.py`, `verify_vulkan_preshaders.py`: ABI, revisão 2 e argumento de estágio quando necessário. Preservar a validação independente por reflexão e checksums; não apenas editar o header de um artefato v1.
- [ ] Escrever `rejects_v1_and_mixed_library`: rejeitar header v1, resultado v1 dentro de header v2, estágio errado, truncamento e metadata incompatível. Atualizar fixtures válidas para v2.
- [ ] Rodar testes de shaders/bootstrap e registrar RED; modificar `binding_contract.h` e `vulkan_shader_service.cpp` via patch privado para exigir revisão 2. Garantir que biblioteca incompatível bloqueia a factory antes de Resume.
- [ ] Rodar testes Python e `tools/test_native.ps1 -Suite shaders -Device RXCWB05KQMX`, `-Suite bootstrap`; exigir GREEN. Preparar/`--verify` snapshots e conferir checkout PC intacto.
- [ ] Commitar somente arquivos desta unidade após a verificação, sem ferramentas privadas ou dados do jogo.


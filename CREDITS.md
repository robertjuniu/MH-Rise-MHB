# Créditos

- **[robertjuniu](https://github.com/robertjuniu)** — manutenção do MH Rise MHB, testes e avaliação do mod no Galaxy S25.
- **[Eden Emulator Project](https://git.eden-emu.dev/eden-emu/eden)**, iniciado por Camille LaVey, e seus contribuidores — emulador usado nos testes. Eden não é um projeto deste repositório nem do seu mantenedor.
- **yuzu e seus contribuidores** — origem de partes do Eden. Os créditos e licenças do emulador continuam pertencendo aos seus autores; o APK não é distribuído aqui.
- **[roccodev/imgui-xeno](https://github.com/roccodev/imgui-xeno)** — backend NVN usado pelo mod, sob GPL v2. Base: `6391c9403c2f8e9c45a420f763ebd26ca03179c0`. As adaptações locais separam a fila e os buffers da HUD, acrescentam sincronização por fences e integração com a captura do Rise.
- **[CraftyBoss](https://github.com/CraftyBoss)** e **[MetroidPrimeModding/mp1r-practice-mod](https://github.com/MetroidPrimeModding/mp1r-practice-mod)** — backend NVN original creditado pelo imgui-xeno.
- **[Amethyst-szs/smo-lunakit](https://github.com/Amethyst-szs/smo-lunakit)** — adaptações e correções aproveitadas pelo backend upstream.
- **[CraftyBoss/MP1R-Exlaunch-Base](https://github.com/CraftyBoss/MP1R-Exlaunch-Base)**, **[shadowninja108/exlaunch](https://github.com/shadowninja108/exlaunch)** e **[EngineLessCC/exlaunch-cmake](https://github.com/EngineLessCC/exlaunch-cmake)** — bases creditadas pelo imgui-xeno; avisos originais preservados.
- **[Omar Cornut e contribuidores do Dear ImGui](https://github.com/ocornut/imgui)** — interface e desenho da HUD, licença MIT. Base: `d4ddc46e7773e9a9b68f965d007968f35ca4e09a`.
- **[cursey](https://github.com/cursey)** — `monster_has_hp_bar.lua`, referência de estudo para a estrutura de HP do Rise no PC, licença MIT. O script não é executado pelo addon de Switch.
- **[GreenComfyTea/MHR-Overlay](https://github.com/GreenComfyTea/MHR-Overlay)** e **[praydog/REFramework](https://github.com/praydog/REFramework)** — referências de estudo da identificação do jogador e dos tipos do jogo; não são dependências de execução deste addon.
- **[HunterPie](https://github.com/HunterPie/HunterPie)** — referência de comportamento da barra solicitada pelo jogador; o mod não incorpora o programa HunterPie.
- **[CrimsonNynja/monster-hunter-DB](https://github.com/CrimsonNynja/monster-hunter-DB)** — catálogo de nomes, categorias e ícones, com licença MIT do catálogo preservada. **[MonsterHunterRiseModding](https://github.com/mhvuze/MonsterHunterRiseModding/wiki/Monster-IDs)** — referência dos IDs.
- **Google e contribuidores de [Roboto](https://github.com/googlefonts/roboto-2)** — fontes Roboto Regular/Bold, licença Apache 2.0.
- **CAPCOM** — Monster Hunter Rise, Sunbreak e as artes dos monstros. A licença do catálogo não transfere os direitos sobre essas artes.
- **LLVM/Android NDK, Pillow, Keystone e Unicorn** — ferramentas usadas para compilar, preparar os recursos e verificar o código ARM64. Não são necessárias para instalar o mod pronto.

Código próprio sob **GPL-2.0-or-later**. O backend imgui-xeno conserva GPL v2 e Dear ImGui conserva MIT. As fontes Roboto e o catálogo mantêm suas licenças próprias. Os textos originais ficam em `LICENSE.txt`, `standalone/vendor/`, `standalone/assets/fonts/` e `references/`.

Os nomes acima indicam a origem do trabalho. Não indicam aprovação oficial nem autoria dessas dependências pelo mantenedor do MH Rise MHB.

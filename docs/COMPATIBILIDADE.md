# Compatibilidade com Eden

MH Rise MHB é um módulo **ExeFS/subsdk9** executado no Switch emulado. Não usa APIs Android, root, modelo de celular ou funções exclusivas do Eden modificado. O renderer chama o NVN exportado pelo próprio jogo.

Foram conferidas todas as **26 tags públicas disponíveis** no espelho do Eden, desde `0.0.0` até `v0.2.1`, incluindo tags de teste e RC, além do `master` consultado em 18/10/2026. Em todas, estavam presentes:

- Carregamento de complementos ExeFS e `subsdk9`.
- Carregador de módulos NSO.
- `GetInfo/MesosphereCurrentProcess`, usado para obter o handle do processo emulado.
- `MapProcessMemory` e `UnmapProcessMemory`, usados para instalar os hooks.
- A reserva inicial de `0x24` bytes no tratamento NCE; o módulo reserva `0x40` antes de suas chamadas SVC.

O resultado por arquivo e versão, com os hashes dos arquivos consultados, está em [eden-interfaces.json](eden-interfaces.json). Fontes: [espelho público do Eden](https://github.com/eden-emulator/mirror) e [repositório oficial](https://git.eden-emu.dev/eden-emu/eden).

Por isso o addon não exige a r4 nem uma versão específica do Eden. Essa conferência de código não equivale a jogar em todas as versões, plataformas e drivers: o teste em jogo foi feito no **Galaxy S25 com Eden Android**. Novas versões e forks podem mudar; problemas podem ser relatados nas Issues com a versão do Eden e do jogo.

O jogo precisa ser **Monster Hunter Rise / Sunbreak de Switch 16.0.2**, ID `0100B04011742000`, build `C9A3DD7702075ECD90101F45553895822804D38F`. A versão exigida é a do jogo, não a do emulador. Antes de modificar as rotinas, o módulo confere suas assinaturas.

Só uma cópia do MHB deve ficar ativa. Não combine com outro mod que substitua `subsdk9` ou altere as mesmas rotinas. O addon não corrige problemas gráficos do jogo ou substitui configurações necessárias para ele rodar no seu aparelho.

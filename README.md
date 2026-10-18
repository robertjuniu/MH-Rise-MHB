# MH Rise MHB (Monster Health Bar)

Mod de barra de vida para **Monster Hunter Rise / Sunbreak de Nintendo Switch**, pensado para jogar em emulador de Switch, principalmente o **Eden**.

É um complemento que você ativa e desativa no Eden. Não precisa instalar um APK modificado.

**[Baixar MH Rise MHB](https://github.com/robertjuniu/MH-Rise-MHB/releases/latest)**

## O que aparece

- Nome, categoria e ícone do monstro grande.
- Barra de vida, porcentagem e vida atual/máxima.
- Rastro do dano recebido na barra.
- HUD compacta no topo central, com contorno e sombras leves para facilitar a leitura.

A barra acompanha o último monstro grande que recebeu dano e fica visível por até **30 segundos** após a última atualização. Ao desmaiar, seu caçador faz a barra sumir após **5 segundos**. Essa regra se repete a cada desmaio; vida baixa e quedas de outros jogadores não a acionam. Uma nova atualização de combate pode exibir a barra novamente.

Ainda não mostra vida de partes, efeitos de status ou vários monstros ao mesmo tempo. A seleção é pelo último dano, não pela distância.

## Compatibilidade

Feito para **Eden**, sem depender de uma versão especial do emulador. Usa o formato de complementos **ExeFS**, presente nas versões públicas conferidas do Eden. As interfaces usadas foram verificadas em todas as 26 tags disponíveis e no código atual; detalhes em [compatibilidade](docs/COMPATIBILIDADE.md).

O requisito de versão é do **jogo**: Monster Hunter Rise / Sunbreak de Switch **16.0.2**, ID `0100B04011742000`. Outros jogos e outras atualizações do Rise não são suportados por este pacote.

**Testado no Samsung Galaxy S25 com Eden Android.** Não há restrição de modelo de celular no mod. Outras versões do Eden e aparelhos não foram todos testados em jogo.

## Instalar

1. Feche o jogo e extraia o ZIP da release.
2. No Eden, abra **Complementos** do Monster Hunter Rise e escolha instalar.
3. Selecione a pasta **MH Rise MHB**, que contém a pasta `exefs`.
4. Ative **MH Rise MHB** e abra o jogo.

Se a sua versão usa instalação manual, coloque a pasta nesta estrutura, dentro da pasta de mods/load do Eden:

```text
load/0100B04011742000/MH Rise MHB/exefs/subsdk9
```

Para desativar, desmarque o complemento e reinicie o jogo. Não mantenha duas cópias do mod ativas. Quem usava o addon de teste `MH-Rise-HUD-Experimental` deve desativá-lo antes de ativar este. Na build com HUD integrado, desative o HUD integrado também.

O mod não substitui saves, shaders, configurações gráficas ou o emulador.

## Código e créditos

Mantido por **[robertjuniu](https://github.com/robertjuniu)**. O código está neste repositório; instruções em [BUILD.md](docs/BUILD.md).

Créditos completos e links em **[CREDITS.md](CREDITS.md)**. O renderer usa imgui-xeno e Dear ImGui; o catálogo de monstros e as fontes mantêm suas licenças. Monster Hunter e seus ícones pertencem à **CAPCOM**.

Projeto independente, sem vínculo oficial com Eden, CAPCOM ou Nintendo. Não inclui jogo, firmware, chaves ou dados de jogadores.

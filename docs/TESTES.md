# Testes

HUD e cenário funcionando juntos no Galaxy S25 com Eden Android; visual e desempenho aprovados pelo jogador. O addon instalado é o mesmo binário incluído na release, SHA-256:

```text
147995f1a296891f3cd7eeae79f5bee0a0271b5bfe6371e6b6054e494e3f38b4
```

O código exportado foi recompilado do zero com o NDK e sem usar arquivos do jogo. Passaram:

- Validação dos segmentos NSO, hashes, chamadas SVC e 276 destinos de relocação.
- 12 cenários ARM64 da captura de HP.
- 9 cenários da proteção de reutilização de buffers da GPU.
- Hooks de desmaio/retorno com registradores e flags preservados.
- Três ciclos de desmaio/retorno em cada índice local 0, 2 e 3, eventos remotos, duplicados e prazos de cinco segundos, usando estruturas simuladas.
- Mil verificações normais da nova regra, sem consultas de memória por SVC ou logs por quadro.

Os logs do aparelho confirmaram o evento local de desmaio. Na v1.4, as amostras com HUD visível registraram preparação média de cerca de 1,68–1,89 ms, sem esperas por reutilização, quadros omitidos ou fallback de sincronização. São amostras curtas: não houve comparação A/B controlada de FPS.

Os testes automatizados não substituem jogar em todos os aparelhos. Não são publicados saves, logs brutos ou outros dados do celular.

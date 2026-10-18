# Compilar e testar

Para instalar, use o ZIP pronto da release. Para compilar, você precisa de Python 3, Android NDK e das dependências Python:

```text
python -m pip install -r requirements.txt
```

Defina `ANDROID_NDK_HOME` apontando para seu NDK. O build foi testado no Windows com NDK `28.2.13676358`; o script também procura os toolchains Linux/macOS, ainda sem validação nesses hosts.

```text
python standalone/build_renderer.py
python standalone/verify_renderer.py
python standalone/verify_capture.py
python standalone/verify_frame_reuse.py
python standalone/verify_player_life.py
```

Saída: `standalone/dist/MH Rise MHB/exefs/subsdk9`.

As dependências de código estão incluídas em `standalone/vendor/`, com as alterações e licenças correspondentes. Não é preciso baixar um SDK proprietário nem extrair o jogo para compilar: os offsets, recursos e nomes das importações usados pelo build 16.0.2 estão no repositório. O NDK fornece o compilador e headers C/C++; o módulo não é um aplicativo Android e não liga bibliotecas Android.

Os testes ARM64 exercitam captura de HP, proteção dos buffers da GPU e vários ciclos de desmaio/retorno, incluindo eventos de outros jogadores. Os testes de tipos de jogador usam estruturas simuladas. Se quiser conferir também as assinaturas originais contra seu próprio dump descomprimido, defina `MHR_MAIN_NSO`. Essa conferência extra é opcional e é marcada como omitida quando o arquivo não existe. Não envie dumps do jogo ao repositório.

O arquivo `addon/MH Rise MHB/exefs/subsdk9` é o binário validado que acompanha a release. Uma recompilação pode mudar o hash por versão do compilador e caminhos gravados em mensagens de diagnóstico.

O log `[MHR-HUD] perf` mostra o custo de preparação da HUD e possíveis esperas de buffers a cada 15 segundos. Isso ajuda a investigar desempenho, mas não é uma medição direta de perda de FPS.

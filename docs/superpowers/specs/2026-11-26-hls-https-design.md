# WiiMC: canais ao vivo (m3u8) e HTTPS

## Objetivo
- Canais ao vivo em m3u8 tocam como um vídeo contínuo.
- Vídeos publicados por HTTPS (ex.: Pixeldrain) abrem no WiiMC.

## Não faz parte
- Fazer o Wii tocar HD, ou codecs que o WiiMC não toca hoje.
- Login em contas Google, arquivos privados, DRM.
- Proxy em outro computador.

## Como é hoje (lido no código, não executado)
- `playtreeparser.c` (parse_m3u) trata todo m3u8 como lista de itens separados.
- `config.h`: HLS_DEMUXER, HTTP_PROTOCOL e TCP_PROTOCOL do FFmpeg estão desligados.
- `stream/http.c` recusa redirecionamento para protocolo diferente de http.
- `settings.h`: validInternetProtocols só tem http e mms.
- O app baixa listas e capas por `utils/http.cpp`, separado do player.
- Nenhuma biblioteca TLS encontrada no projeto.
- `.github/workflows/build.yml` compila no contêiner ghcr.io/extremscorner/libogc2.

## Restrições
- Wii: pouca memória e CPU.
- Sem compilador no ambiente de desenvolvimento: compilação pelo GitHub Actions
  de um fork do usuário; teste no Wii dele.
- O repositório original não é do usuário. Licença GPL.

## Abordagens
- A. Leitor HLS próprio como "stream" do MPlayer. Usa a rede atual; criptografia
  e áudio separado exigem trabalho extra.
- B. Ligar o HLS do FFmpeg embutido (`libavformat/hls.c`). Traz AES-128; é
  antigo e exige ligar a camada de rede do FFmpeg, não usada hoje.
- C. Proxy na rede local. Não altera o WiiMC; exige aparelho ligado.

**Escolha do usuário: A e B.** C fica de fora.

## Entregas (proposta do desenvolvedor)
Os dois exemplos do usuário são https, então o HTTPS vem primeiro.

0. Compilar o WiiMC sem alterações no fork, para validar o caminho.
1. HTTPS no player; teste com o vídeo de exemplo.
2. Build A (m3u8) sobre a etapa 1.
3. Build B (m3u8) em outra branch, também sobre a etapa 1; o TLS precisa ser
   ligado também à rede do FFmpeg.
4. O usuário compara A e B; o melhor segue. HTTPS nas listas e capas do app.

## Comportamento do m3u8 (proposta)
- É HLS quando o conteúdo tem tags `#EXT-X-`; outros m3u/m3u8 (listas de canais)
  continuam como hoje.
- Com várias qualidades: a maior com altura até 480; sem essa informação, a de
  menor bitrate. Se só houver HD, tenta a menor.
- Ao vivo: recarrega a lista, não repete pedaços, tenta de novo em falhas de rede.
  Sem avanço/retrocesso.
- Pedaços MPEG-TS com H.264/AAC. fMP4 e áudio em faixa separada ficam de fora.
- A: sem criptografia na primeira versão, com mensagem de erro clara.
  B: o AES-128 que o hls.c já traz, se compilar.

## HTTPS (proposta)
- TLS 1.2 no mínimo, com SNI; mbedTLS como primeira candidata (do contêiner, se
  houver; senão incluída no projeto).
- Vale para o player, as listas e capas do app, e para redirecionamentos
  http <-> https.
- Certificados verificados por padrão; opção para desligar, porque o relógio do
  Wii errado faz a verificação falhar.
- Google Drive fica fora da primeira versão: o usuário não o usou no exemplo por
  uma restrição de 100 MB. A referência de teste é o Pixeldrain.
- Avanço em vídeo gravado depende de o servidor aceitar Range.

## Como saber que deu certo
- Um canal ao vivo toca por 15 minutos seguidos sem voltar à lista.
- Um vídeo SD em HTTPS toca do começo ao fim.
- Arquivos locais, http e playlists m3u comuns continuam funcionando.
- Testes no computador para a leitura do m3u8, onde der para isolar o código.

## Diagnóstico (decisão do desenvolvedor)
- Os builds de teste gravam um log no cartão SD, desde o primeiro, porque os
  erros só podem ser observados no Wii do usuário.

## Fica a critério do desenvolvedor
Nomes de arquivos, tamanhos de buffer, organização do código e commits.

## Pendente
Exemplos do usuário:
- Canal ao vivo m3u8 em https (link enviado na conversa).
- Vídeo em https no Pixeldrain (link enviado na conversa).

Nenhum dos dois pôde ser lido do ambiente de desenvolvimento. Faltam: qualidade,
codecs e criptografia do canal; formato, tamanho e resolução do vídeo.
Premissa: o usuário consegue enviar o log do cartão SD.

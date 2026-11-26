# Plano: Etapa 1 (HTTPS no player) e identidade WitFLIX

Spec: ../specs/2026-11-26-hls-https-design.md. Execução direta, aprovada com "pode seguir".

1. Sondar o contêiner de build: existe mbedTLS? Qual API de rede e entropia há?
2. Camada TLS (`source/utils/tls.{c,h}`): conectar, ler, escrever, fechar, por fd;
   SNI, verificação de certificado com opção de desligar, log no SD.
3. Player: `stream/http.c`, `stream/network.c`, `url.c` aceitam https (porta 443),
   leem/escrevem pela camada TLS e seguem redirecionamentos http <-> https.
4. App: aceitar `https` nos protocolos permitidos.
5. Testar no computador o que der para isolar; compilar no GitHub Actions.
6. Identidade WitFLIX: nome, descrição e ícone próprios (créditos GPL mantidos).

Etapas A e B (m3u8) vêm depois do teste do HTTPS no Wii.

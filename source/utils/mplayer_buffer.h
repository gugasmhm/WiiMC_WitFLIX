/* Tamanho do buffer de rede/disco do MPlayer e da area MEM2 que o guarda. */
#ifndef MPLAYER_BUFFER_H
#define MPLAYER_BUFFER_H

#define MPLAYER_BUFFER_SIZE (8*1024*1024)
/* a area precisa ser maior que o buffer: o controle do heap ocupa parte dela */
#define MPLAYER_AREA_SIZE (MPLAYER_BUFFER_SIZE + 64*1024)

#endif

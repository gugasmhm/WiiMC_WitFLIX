#!/bin/bash
# Compila os trechos do FFmpeg que usam "paired singles" (a aceleracao de
# video/audio do processador do Wii). O GCC atual removeu a opcao -mpaired,
# entao estes arquivos sao compilados com um devkitPPC antigo (GCC 8) e os
# objetos sao reaproveitados pela compilacao normal.
#
# Rodar a partir da raiz do repositorio, dentro de devkitpro/devkitppc:20190212.
set -e

ROOT=$(pwd)
LIST="libavcodec/binkdsp.o libavcodec/imgconvert.o"
for f in h264dsp h264pred vc1dsp fft mdct rdft dsputil float fmtconvert; do
	LIST="$LIST libavcodec/ppc/${f}_paired.o"
done

cd source/mplayer/ffmpeg
make $LIST \
	CC="$DEVKITPPC/bin/powerpc-eabi-gcc -mpaired" \
	LIBOGC_INC="$DEVKITPRO/libogc/include" \
	PORTLIBS="$DEVKITPRO/portlibs/ppc"

for o in $LIST; do
	n=$("$DEVKITPPC/bin/powerpc-eabi-objdump" -d "$o" | grep -cE '\s(ps_|psq_)' || true)
	echo "$o: $n instrucoes paired singles"
	[ "$n" -gt 0 ] || { echo "ERRO: $o sem aceleracao"; exit 1; }
	rm -f "${o%.o}.d"
done

tar -C "$ROOT" -cf "$ROOT/paired-objs.tar" $(printf 'source/mplayer/ffmpeg/%s ' $LIST)

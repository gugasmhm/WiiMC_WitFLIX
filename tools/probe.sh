cat > /tmp/t.c <<'C'
#include <paired.h>
#include "libavutil/ppc/paired.h"
void copy(unsigned char *s, unsigned char *d){ vector float p[2]; p[0]=psq_l(0,s,0,4); p[1]=p[0]; vector float r=p[1]; psq_st(r,0,d,0,4); }
C
cd source/mplayer
cp /tmp/t.c ffmpeg/t_probe.c
B="-I. -Iffmpeg -DHAVE_AV_CONFIG_H -I$DEVKITPRO/libogc/include -std=gnu99 -S -o -"
G=$DEVKITPPC/bin/powerpc-eabi-gcc
$G --version | head -1
grep -n 'define vector\|vector' $DEVKITPRO/libogc/include/paired.h $($G -print-file-name=include)/paired.h 2>/dev/null | head -8
run(){ echo "=== $*"; $G $B "$@" ffmpeg/t_probe.c 2>&1 | grep -vE '^\s*\.' | head -30; }
run -O2 -mrvl -mcpu=750 -meabi -mhard-float -mpaired
run -O2 -mcpu=750 -mpaired -mrvl -meabi
run -O3 -mpaired -mcpu=750 -meabi -mrvl -mhard-float

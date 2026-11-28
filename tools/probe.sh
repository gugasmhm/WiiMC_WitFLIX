cat > /tmp/t.c <<'C'
typedef float v2 __attribute__((vector_size(8)));
void copy(unsigned char *s, unsigned char *d){ v2 p[2], r; asm volatile("psq_l %0,0(%1),0,4":"=f"(p[0]):"b"(s)); p[1]=p[0]; r=p[1]; asm volatile("psq_st %0,0(%1),0,4"::"f"(r),"b"(d):"memory"); }
v2 g; void mv(v2 *a){ v2 t=*a; asm volatile("# keep"::: "fr1","fr2"); g=t; }
C
CODE=$(. /etc/os-release; echo $VERSION_ID)
if [ "$CODE" = 8 ]; then S=jessie; PKG=gcc-4.9-powerpc-linux-gnu; else S=stretch; PKG=gcc-powerpc-linux-gnu; fi
echo "deb http://archive.debian.org/debian $S main" > /etc/apt/sources.list
rm -f /etc/apt/sources.list.d/*
apt-get -o Acquire::Check-Valid-Until=false update -qq 2>&1 | tail -2
apt-get install -y -qq $PKG 2>&1 | tail -3
G=$(ls /usr/bin/powerpc-linux-gnu-gcc* | grep -E 'gcc(-[0-9.]+)?$' | head -1)
echo "PROBE $S $($G --version | head -1)"
$G -mcpu=750 -mpaired -O3 -meabi -mhard-float -S -o - /tmp/t.c 2>&1 | grep -vE '^\s*\.' | sed 's/^/PROBE /'

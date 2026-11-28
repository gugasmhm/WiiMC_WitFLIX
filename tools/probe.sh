cat > /tmp/t.c <<'C'
typedef float v2 __attribute__((vector_size(8)));
void copy(unsigned char *s, unsigned char *d){ v2 p[2], r; asm volatile("psq_l %0,0(%1),0,4":"=f"(p[0]):"b"(s)); p[1]=p[0]; r=p[1]; asm volatile("psq_st %0,0(%1),0,4"::"f"(r),"b"(d):"memory"); }
v2 g; void mv(v2 *a){ v2 t=*a; asm volatile("# keep"::: "fr1","fr2"); g=t; }
C
apt-get update -qq 2>&1 | tail -2
for V in 4.7 4.8; do
  apt-get install -y -qq gcc-$V-powerpc-linux-gnu 2>&1 | grep '^E' 
  G=powerpc-linux-gnu-gcc-$V
  echo "PROBE == $($G --version | head -1)"
  $G -mcpu=750 -mpaired -O3 -meabi -mhard-float -S -o - /tmp/t.c 2>&1 | grep -vE '^\s*\.' | sed 's/^/PROBE /'
done

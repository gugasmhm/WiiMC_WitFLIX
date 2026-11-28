bash tools/build-paired.sh >/dev/null 2>&1
for o in $(tar -tf paired-objs.tar); do
  $DEVKITPPC/bin/powerpc-eabi-objdump -d -M 750cl --no-show-raw-insn $o | awk -v O=$(basename $o) '
   /^[0-9a-f]+ <.*>:$/ {f=$2; next}
   NF>1 { op=$2; a=$3
     if (op=="lwz" && prev=="stfd") { split(pa,x,","); split(a,y,","); if (x[2]==y[2]) c[f]++ }
     if (op ~ /^ps/) p[f]++
     prev=op; pa=a }
   END { for (f in p) printf "AUD %s %s ps=%d copias=%d\n", O, f, p[f], c[f]+0 }'
done

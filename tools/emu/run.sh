# roda um DOL no Dolphin sem tela e mostra as linhas EMUTEST
D=$(mktemp -d); mkdir -p $D/Config
printf '[Options]\nWriteToConsole = True\nWriteToFile = True\nVerbosity = 4\n[Logs]\nOSREPORT = True\nOSREPORT_HLE = True\n' > $D/Config/Logger.ini
BIN=$(command -v dolphin-emu-nogui || echo /usr/games/dolphin-emu-nogui)
timeout ${2:-60} $BIN -u $D -p headless -v Null -a HLE -e "$1" > $D/out.txt 2>&1 &
P=$!
for i in $(seq 1 ${2:-60}); do sleep 1; grep -qs 'EMUTEST FIM' $D/out.txt $D/Logs/dolphin.log && break; done
kill $P 2>/dev/null
cat $D/out.txt $D/Logs/dolphin.log 2>/dev/null | grep -o 'EMUTEST.*' | sort -u
echo "--- saida do emulador"; head -15 $D/out.txt

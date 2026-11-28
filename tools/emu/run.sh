#!/bin/bash
# Roda um DOL de teste no Dolphin sem tela. Falha se algum teste falhar ou se o
# programa nao chegar ao fim.  Uso: run.sh arquivo.dol [segundos]
D=$(mktemp -d); mkdir -p "$D/Config"
printf '[Options]\nWriteToConsole = True\nVerbosity = 4\n[Logs]\nOSREPORT = True\nOSREPORT_HLE = True\n' > "$D/Config/Logger.ini"
BIN=$(command -v dolphin-emu-nogui || echo /usr/games/dolphin-emu-nogui)
timeout "${2:-90}" "$BIN" -u "$D" -p headless -v Null -a HLE -e "$1" > "$D/out.txt" 2>&1 &
P=$!
for i in $(seq 1 "${2:-90}"); do sleep 1; grep -aqs 'EMUTEST FIM' "$D/out.txt" && break; done
kill $P 2>/dev/null
grep -a -h -o 'EMUTEST.*' "$D/out.txt" | tee "$D/res.txt"
grep -q 'EMUTEST FIM' "$D/res.txt" || { echo "ERRO: o teste nao terminou"; head -20 "$D/out.txt"; exit 1; }
! grep -q 'EMUTEST FALHOU' "$D/res.txt"

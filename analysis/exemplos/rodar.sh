#!/usr/bin/env bash
# Runs the whole example of .ai/docs/fluxo-analise-offline.md: preparar.sh, then steps 2-9.
# Run in the container, as admin. Results in calib_runs/analises/exemplo/saida/.
set -uo pipefail
H=$(cd "$(dirname "$0")" && pwd)
bash "$H/preparar.sh" || exit 1
cd "$H/.."
run() { echo; echo "=== $1"; python3 -m zedcal.$2 --config "$H/$1.yaml"; }
run 01_calibrate_v2 calibrate
run 02_cobertura_v2 coverage_command || echo "(cobertura reprovada: veja coverage.json)"
run 03_m1_v2_propria m1_command
run 04_m1_v2_fabrica m1_command
run 05_m4_v2 m4_command
run 06_m2 m2_command
run 07_m3 m3_command
run 08_relatorio report_command

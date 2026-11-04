# Plano — Estatística por condição e comparação

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- As saídas de M1–M4 em formato estável (os specs respectivos).
- Margens aprovadas pelo usuário, para a etapa 4. Até lá, os testes usam margens de exemplo.

## Etapas

- [x] 1. **Estatística descritiva** (R1, R2). `zedcal/stats.py`: `calibration_quantities` (R pelo
  vetor de Rodrigues em graus) e `describe`. O CV é "não informativo" para ty, tz e os ângulos, e
  sempre que o IC95 % contém 0. Testes em `tests/test_stats.py`.

  Especificação original da etapa: Média, IC95 % t, CV, a marca "não informativo", e a
  posição e o percentil de F.
  - Testes primeiro: o caso do IC calculado à mão; uma grandeza com média ≈ 0 é marcada.
  - **Pronto quando:** `pytest` verde.
- [x] 2. **Permutação exata** (R3). `permutation_test`. A matriz do M2 não é simétrica (A→B ≠ B→A
  por pouco); o teste usa (M + Mᵀ)/2. A enumeração é vetorizada: ~0,23 s por teste 20 × 20 (as duas
  variantes). As 200 repetições do critério de uniformidade levam ~46 s e rodam com
  `ZEDCAL_SLOW=1` (KS p > 0,01, verificado em 2026-11-03); sem ela, 40 repetições.

  Especificação original da etapa: Enumeração das 184 756 atribuições (e das 92 378 divisões, na
  variante simétrica) e as duas estatísticas.
  - Testes primeiro: o critério da uniformidade de p e o do deslocamento; a contagem de divisões.
  - **Pronto quando:** `pytest` verde, e o tempo para uma matriz 20 × 20 é registrado.
- [x] 3. **Aplicação cruzada** (R4). `cross_application`: por calibração, a média nas capturas da
  outra condição menos a média nas da própria, sem a própria captura (treino); IC t sobre as
  calibrações. Consome qualquer matriz calibração × captura (as do M3; do M4, montada da mesma
  forma).

  Especificação original da etapa: Consome as matrizes do M3 e do M4.
  - **Pronto quando:** com dois conjuntos sintéticos, a diferença e o IC saem corretos.
- [x] 4. **TOST** (R5), com as margens do arquivo de configuração. `tost` (Welch). As margens
  continuam **provisórias** (as da proposta do spec, não aprovadas); o relatório diz isso.
  - Poder simulado com n = 10 por condição, sem diferença real, margem de fx ±2,7 px: 100 % com
    DP de 1 px, 81 % com 2 px, 25 % com 3 px.

  Especificação original da etapa: TOST, com as margens do arquivo de configuração.
  - Testes primeiro: os casos do critério de aceitação.
  - **Pronto quando:** `pytest` verde.
- [x] 5. **Levene e F** (R6), com o ajuste de Holm (R7). `dispersion` e `holm`.
  - **Achado:** com n = 10, o Brown–Forsythe tem pouco poder. Num caso determinístico com o DP
    dobrado, p = 0,07 (o F dá p < 0,05). Simulado (4 000 repetições, α = 0,05):

    | DP B / DP A | F, n = 10 | BF, n = 10 | F, n = 20 | BF, n = 20 |
    |---|---:|---:|---:|---:|
    | 1,0 | 0,05 | 0,03 | 0,05 | 0,04 |
    | 1,5 | 0,31 | 0,13 | 0,53 | 0,30 |
    | 2,0 | 0,64 | 0,31 | 0,89 | 0,69 |
    | 3,0 | 0,93 | 0,64 | 1,00 | 0,97 |

    O F é mais poderoso, mas pressupõe normalidade; o BF é robusto e fraco. O critério de
    aceitação ("o DP dobrado é detectado") vale para o F com n = 10 e para os dois com n = 20.
    Entra no `experimentos` como limite do protocolo: com 10 capturas, só um aumento de dispersão
    de ~3× é detectado com segurança.

  Especificação original da etapa: Levene e F, com o ajuste de Holm (R7).
  - Testes primeiro: o caso do desvio-padrão dobrado; Holm num conjunto conhecido.
  - **Pronto quando:** `pytest` verde.
- [ ] 6. **Relatórios** (R8). `python3 -m zedcal.report_command --config ...`: `report.md`,
  `report.json`, `parameters.png` e `permutation.png`. Calcula a matriz do M2 e o EME a partir
  das calibrações (região coberta quando há detecções); M3 e M4 entram pelos JSON. O relatório de
  duas condições sintéticas de 10 calibrações sai em ~1,4 s (teste
  `test_report_of_two_synthetic_conditions`). **Falta a revisão do formato pelo usuário.**

  Especificação original da etapa: Por condição e por comparação, em Markdown com figuras.
  - **Pronto quando:** o relatório das duas condições sintéticas é gerado e revisado pelo usuário
    quanto ao formato.

## Riscos

- **Com n = 10, o TOST tem pouco poder:** a equivalência pode não ser declarada mesmo sem
  diferença real. O relatório mostra o IC90 % junto da margem, para que se veja se faltou dado ou
  se a diferença existe.

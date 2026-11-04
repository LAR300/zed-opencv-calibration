# Plano — Tempo até o regime térmico

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído.
- [`ambiente-sensores`](../ambiente-sensores/plan.md) aplicado, para ter a temperatura.
- F exportada.

## Etapas

- [ ] 1. **Modo `--timelapse`** (R1). `--timelapse <intervalo_s> <duracao_min>`, só com
  `--session`. `common/capture_io/timelapse.{hpp,cpp}` (`TimelapseSchedule`): horários ancorados no
  início (início + k·intervalo), sem acumular o atraso do laço; um horário perdido por inteiro é
  pulado e contado. Testes em `tests/test_timelapse.cpp` (166/166): 30 s × 60 min dá 120 pares
  espaçados 29–31 s com o laço de 10–80 ms.
  - No laço da coleta: a agenda substitui o `s` (`s`, espaço e `f` ignorados); o filtro de
    dissimilaridade e o de nitidez não se aplicam (a análise julga os pares); `pose_id =
    thermal`; sem métricas de cobertura nem máximo; no fim, `end_reason = timelapse complete`
    e sai sem calibrar. Um par sem o alvo nas duas câmeras não é salvo, com aviso.
  - Verificado ao vivo em 2026-11-04, sem o alvo (em casa): `--timelapse 3 0.25` agendou 5 pares,
    avisou "target not seen" em cada um, terminou sozinho com `end_reason` certo; os erros de
    argumentos saem com mensagem. **Falta na bancada** o critério de 120 pares com o alvo.

  Especificação original da etapa: Grava um par por intervalo, sem a tecla `s`, com
  `pose_id = thermal` e os metadados completos.
  - **Pronto quando:** o critério de 120 pares com o espaçamento de 30 ± 1 s passa numa execução de
    60 min.
- [x] 2. **Análise** (R2). `zedcal/thermal.py` (`series`) e
  `python3 -m zedcal.thermal_command --config ...` (vários registros, F fixa). Por par: translação
  do alvo, rotação **relativa ao primeiro par** (o vetor de Rodrigues absoluto de um alvo de
  frente para a câmera fica perto de π e troca de sinal), RMS e resíduo médio com sinal da
  transferência para a direita, e resíduo médio da esquerda. Tempo pelo `timestamp_ns` do
  `meta.json`; sem ele, pelo índice do par, com aviso. Testes em `tests/test_thermal.py`.

  Especificação original da etapa: `python3 -m zedcal.thermal <captura>`: pose, transferência
  estéreo e resíduo médio, por par.
  - Testes primeiro: com dados sintéticos sem deriva, as séries são planas dentro do ruído.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Critério de regime** (R3). `steady_time`. Ruído σ de um par pelas diferenças
  consecutivas (RMS aparado em 5 MAD-σ, / √2). Uma janela está assentada quando a sua média fica a
  até `factor`·σ (padrão 2) da média da última janela; o regime é o centro da primeira janela a
  partir da qual todas estão assentadas, **desde que** a reta ajustada ao último terço do
  registro mude no máximo `factor`·σ (sem isso, uma deriva que dura até o fim pareceria assentada
  contra a última janela).
  - Simulado (deriva 0,5·e^(−t/8 min), σ = 0,05, um par a cada 30 s, 60 min, 200 repetições):
    regime verdadeiro 12,9 min; média estimada 14,0 min; 94,5 % a menos de uma janela (5 min).
    Uma deriva linear de 0,01/min (2σ a cada 10 min) é dada como "sem regime" em 100 %; de
    0,005/min, em 56 %.
  - O critério do spec ("erro < 1 janela") vale assim em ~95 % dos registros, não em todos; o
    teste exige ≥ 90 % em 100 repetições.

  Especificação original da etapa: Critério de regime (R3).
  - Testes primeiro: o caso da deriva exponencial sintética.
  - **Pronto quando:** `pytest` verde.
- [ ] 4. **Três registros a frio** (R4, R5). Em dias diferentes, com a câmera desligada há pelo
  menos 2 h.
  - **Pronto quando:** o relatório tem as curvas e o X proposto, e o usuário o aprovou.

## Riscos

- **A cena pode mudar durante 60 min:** luz do dia, vibração da mesa. Bancada em sala com luz
  artificial fixa; registrar a hora; descartar o registro se a pose saltar.

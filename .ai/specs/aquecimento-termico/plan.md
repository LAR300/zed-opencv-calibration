# Plano — Tempo até o regime térmico

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído.
- [`ambiente-sensores`](../ambiente-sensores/plan.md) aplicado, para ter a temperatura.
- F exportada.

## Etapas

- [ ] 1. **Modo `--timelapse`** (R1). Grava um par por intervalo, sem a tecla `s`, com
  `pose_id = thermal` e os metadados completos.
  - **Pronto quando:** o critério de 120 pares com o espaçamento de 30 ± 1 s passa numa execução de
    60 min.
- [ ] 2. **Análise** (R2). `python3 -m zedcal.thermal <captura>`: pose, transferência estéreo e
  resíduo médio, por par.
  - Testes primeiro: com dados sintéticos sem deriva, as séries são planas dentro do ruído.
  - **Pronto quando:** `pytest` verde.
- [ ] 3. **Critério de regime** (R3).
  - Testes primeiro: o caso da deriva exponencial sintética.
  - **Pronto quando:** `pytest` verde.
- [ ] 4. **Três registros a frio** (R4, R5). Em dias diferentes, com a câmera desligada há pelo
  menos 2 h.
  - **Pronto quando:** o relatório tem as curvas e o X proposto, e o usuário o aprovou.

## Riscos

- **A cena pode mudar durante 60 min:** luz do dia, vibração da mesa. Bancada em sala com luz
  artificial fixa; registrar a hora; descartar o registro se a pose saltar.

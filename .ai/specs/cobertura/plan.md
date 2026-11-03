# Plano — Verificação de cobertura

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md), etapas 1–6.
- A definição de ângulos de [`roteiro-poses`](../roteiro-poses/spec.md) (condições de
  implementação). O teste de paridade (etapa 3) precisa do `common/pose_script/` C++.

## Etapas

- [ ] 1. **Grade de densidade** (R1, R2). Contagem por célula e por câmera, critério N e células de
  canto.
  - Testes primeiro: uma grade sintética conhecida dá as contagens exatas; um canto no limite de
    duas células cai numa só, com a regra documentada.
  - **Pronto quando:** `pytest` verde.
- [ ] 2. **Inclinações** (R3, R4). Pose PnP, os dois ângulos, histogramas e critério com K e sinais.
  - Testes primeiro: poses sintéticas com ângulos conhecidos dão erro < 0,1°; o critério reprova a
    falta de um eixo ou de um sinal.
  - **Pronto quando:** `pytest` verde.
- [ ] 3. **Paridade com o C++.** Um fixture com a pose sintética e os ângulos esperados é usado
  pelos dois testes, o C++ e o Python.
  - **Pronto quando:** os dois passam com o mesmo fixture.
- [ ] 4. **Relatório e código de saída** (R5–R7). `python3 -m zedcal.coverage <captura>`, com JSON,
  figuras e código de saída.
  - **Pronto quando:** os três casos sintéticos dos critérios dão os motivos certos e os códigos
    0 e ≠ 0.
- [ ] 5. **Capturas existentes.** Rodar nas 3 capturas AprilGrid e registrar as distribuições de
  contagem no resultado do piloto, para a escolha de N.
  - **Pronto quando:** os mapas estão em `calib_runs/piloto/`, e N foi escolhido pelo usuário e
    gravado na config padrão.

## Riscos

- **N é escolhido depois das capturas existentes.** Elas não seguiram o roteiro, então a
  distribuição pode ser mais pobre que a de uma captura com roteiro. Rever N depois da primeira
  captura completa com roteiro.

# Plano — Captura estática

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído.

## Etapas

- [ ] 1. **Acompanhamento de cantos** (R1). `StabilityTracker`: recebe as detecções E/D por quadro,
  com o timestamp; devolve o deslocamento RMS das tags comuns ao quadro anterior e o tempo estável
  acumulado.
  - Testes primeiro: os três casos sintéticos do critério de imobilidade.
  - **Pronto quando:** `ctest` verde.
- [ ] 2. **Nitidez por polígono** (R2). Variância do Laplaciano na máscara dos polígonos das tags.
  - Testes primeiro: os dois casos sintéticos do critério de nitidez.
  - **Pronto quando:** `ctest` verde.
- [ ] 3. **Seção `static_capture` da sessão** (R3), com validação.
  - Testes primeiro: valores ausentes usam os padrões; valores ≤ 0 são recusados.
  - **Pronto quando:** `ctest` verde.
- [ ] 4. **Modo `--measure_static_noise`** (R7). Com o alvo parado, grava N segundos de deslocamento
  e de nitidez em `noise.json`.
  - **Pronto quando:** na bancada, o arquivo existe com percentis, e os limiares padrão de
    `config/session_example.yaml` são atualizados a partir dele.
- [ ] 5. **Integração na coleta** (R4–R6, R8). Painel, bloqueio do `s`, quadro salvo do fim da
  janela, campos no `meta.json`.
  - **Pronto quando:** o teste de bancada do critério de aceitação passa e a regressão do xadrez é
    idêntica.

## Riscos

- **Com as inclinações fortes do bloco 2, poucas tags comuns** deixam o deslocamento RMS ruidoso.
  Exigir um mínimo de cantos comuns no acompanhamento e mostrar "indeterminado" abaixo dele.

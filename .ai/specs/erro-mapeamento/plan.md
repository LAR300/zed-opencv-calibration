# Plano — M2: erro de mapeamento e EME empírico

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md), etapas 4–7: YAML, projeção, sintético e calibração.

## Etapas

- [ ] 1. **Grade e desprojeção controlada** (R1, R2). Grade configurável; `undistortPointsIter`
  com critério reforçado; ida e volta com A e exclusão dos pontos acima do limiar.
  - Testes primeiro: sem distorção, a ida e volta é exata; num modelo com k1 extremo, os pontos
    excluídos ficam só na borda e são contados.
  - **Pronto quando:** `pytest` verde.
- [ ] 2. **Erro A → B** (R1, R3). RMS e máximo nas duas convenções, mais o mapa por ponto.
  - Testes primeiro: os casos "idênticas" e "fx perturbado" dos critérios.
  - **Pronto quando:** `pytest` verde.
- [ ] 3. **Variante com rotação** (R4). `least_squares` sobre 3 parâmetros de rotação.
  - Testes primeiro: o caso "cx deslocado"; uma rotação imposta de 0,1° entre A e B (raios
    rodados) é recuperada até 1e-4°.
  - **Pronto quando:** `pytest` verde.
- [ ] 4. **Referência da condição** (R5). A média no espaço da imagem com o ajuste `rational8`, a
  média de parâmetros e a calibração conjunta, via núcleo.
  - Testes primeiro: o critério das 10 calibrações sintéticas; o ajuste à média de 10 cópias de um
    mesmo modelo devolve esse modelo.
  - **Pronto quando:** `pytest` verde.
- [ ] 5. **EME empírico, matriz de pares e F** (R6, R7).
  - Testes primeiro: n = 10 dá 45 pares; o percentil de F é calculado sobre os 10 valores e
    testado com um conjunto conhecido.
  - **Pronto quando:** `pytest` verde.
- [ ] 6. **Comando, figuras e config** (R3, R8). `python3 -m zedcal.m2 --config ...`; mapas de calor
  por par e por câmera.
  - **Pronto quando:** roda com as calibrações reais existentes e F, e produz a matriz e os mapas.

## Riscos

- **O ajuste `rational8` à média pode ser mal condicionado** nos coeficientes altos. Inicializar
  com a média de parâmetros e conferir o resíduo do ajuste. Esse resíduo também vai para o
  relatório.

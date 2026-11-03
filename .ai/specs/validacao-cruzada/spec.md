# Spec — M3: generalização por validação cruzada entre capturas

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R4, M3; R5 (F nas mesmas imagens de validação); R6 (2), aplicação cruzada entre condições.
> Depende de [`nucleo-offline`](../nucleo-offline/spec.md).

## Problema

O RMS de treino mede o ajuste aos próprios dados e cai com qualquer modelo flexível. O que interessa
ao SLAM é se a calibração **generaliza** para imagens que ela não viu, inclusive nos extrínsecos.

Antes disso só houve validações cruzadas feitas à mão, com scripts descartáveis que detectam em
Python (`calib_runs/tools/crossval*.py`; auditoria, risco 6).

## Requisitos

- **R1. Avaliação de uma calibração C nas imagens de uma captura J.** Para cada par de J:
  1. **pose do alvo por PnP na esquerda,** com os intrínsecos de C fixos e as tags da esquerda;
     IPPE + LM;
  2. **RMS da esquerda** nessa pose;
  3. **RMS de transferência na direita:** a pose da esquerda composta com R, T de C, projetada com
     os intrínsecos da direita, contra as tags da direita;
  4. **RMS estéreo combinado:** √((e_E² + e_D²)/2), a mesma definição do
     `zed_stereo_checker` (`stereo_checker/src/main.cpp:572-574`).

  Convenção `per_corner_2d`. A saída traz também a mediana e o percentil 90 por vista.
- **R2. Matriz leave-one-capture-out.** Para as n capturas de uma condição (n = 10), a matriz
  n × n com C = a calibração da captura i e J = a captura j, para cada métrica de R1.
  - A diagonal é o treino; fora dela, o teste.
  - **Saída:** as matrizes, a média da diagonal, a média fora da diagonal e a razão entre elas.
- **R3. Calibrações extras como linhas da matriz:** F e a "média" da condição
  ([`erro-mapeamento`](../erro-mapeamento/spec.md) R5), avaliadas em todas as capturas.
- **R4. Aplicação cruzada entre condições** (R6 (2) do brief): as calibrações da condição A
  avaliadas nas capturas da condição B, e vice-versa. Saída: a matriz n_A × n_B e o resumo.
- **R5. Por distância e por região.** As métricas de R1 também são agregadas por faixa de
  distância (pela translação do PnP) e por célula da grade 8×8 da imagem.
- **R6. Configuração** em YAML: as capturas, as calibrações, as faixas e a grade. A saída vai para
  `<condicao>/metricas/m3/`.

## Condições de implementação

- **A calibração de cada captura é a variante escolhida para os experimentos** (decisão do
  piloto, registrada em [`experimentos`](../experimentos/spec.md)). O M3 recebe as calibrações
  prontas e não recalibra.
- **Por que PnP na esquerda e transferência na direita:** a direita testa R e T sem reajuste. É
  como o SLAM usa a calibração. É também o que o brief pede.

## Não-objetivos

- Calibrar: as calibrações vêm do núcleo.
- Erro de triangulação e erro epipolar (M4).

## Critérios de aceitação

- [ ] **Diagonal = treino:** com i = j, o RMS da esquerda reproduz o RMS da esquerda com pose livre
      do M1 para a mesma calibração e as mesmas detecções, até 1e-6 px.
- [ ] **Dados sintéticos:**
  - uma calibração com o modelo verdadeiro avaliada em outra captura sintética dá RMS ≈ σ√2;
  - uma calibração com fx 1 % errado dá RMS fora da diagonal maior que o da verdadeira;
  - um T com 1 mm a mais em x aumenta só o erro de transferência da direita, não o da esquerda.
- [ ] F e a média aparecem como linhas da matriz.
- [ ] A aplicação cruzada A × B com dois conjuntos sintéticos produz a matriz n_A × n_B.

## Perguntas em aberto

Nenhuma.

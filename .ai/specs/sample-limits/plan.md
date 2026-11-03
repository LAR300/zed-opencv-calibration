# Plano — Limites de amostras configuráveis e encerramento manual

> **Como implementar**, em micro-tarefas verificáveis. Marque `[x]` ao concluir. O porquê das
> escolhas está em [`decisions.md`](decisions.md) (Dn); os requisitos em [`spec.md`](spec.md) (Rn).

## Pré-requisitos

- Branch `sample-limits`, criada a partir da `aprilgrid-target`.
- Container `isaac-ros-dev`, entrada sempre como `admin`. Build:
  `cd $REPO/build && cmake .. [-DBUILD_TESTS=ON] && make -j$(nproc)`.
- A linha de base de regressão do xadrez em `calib_runs/regress_chess/` (`baseline.log`,
  `baseline.yml`), da spec `aprilgrid-target`.
- ZED 2i e o alvo AprilGrid para as etapas 6 e 7.

## Etapas

### Fase A — regras puras (TDD, sem câmera)

- [x] 1. **Validação dos limites** (D2, D4; R1, R2). Em `stereo_calibration/include/sample_limits.hpp`:
  `parseSampleCount(text, value, error)` e `validateSampleLimits(min, max, error)`, mais as
  constantes `COLLECTION_MIN_SAMPLES = 25`, `COLLECTION_MAX_SAMPLES = 35` e
  `MIN_CALIBRATION_SAMPLES = 20`, que reaproveita `MIN_IMAGE`. Incluir o diretório em
  `tests/CMakeLists.txt`.
  - Testes primeiro:
    - `"25"` aceito; `"abc"`, `"25x"`, `""` e `"-3"` recusados com mensagem;
    - mínimo 19 recusado e mínimo 20 aceito;
    - máximo 0 aceito (sem máximo);
    - máximo 24 com mínimo 25 recusado, e máximo 25 com mínimo 25 aceito.
  - **Verificação:** `ctest` verde.
- [x] 2. **Decisão da coleta** (D1, D2; R4–R6). `collectionStep(count, min, max, metrics_complete,
  max_lifted)` devolve `CONTINUE`, `COMPLETE` ou `PAUSE_AT_MAX`.
  - Testes primeiro:
    - métricas completas com `count >= min` → `COMPLETE`;
    - métricas completas com `count < min` → `CONTINUE`;
    - `count == max` com métricas incompletas → `PAUSE_AT_MAX`;
    - `max == 0` → nunca `PAUSE_AT_MAX`;
    - `max_lifted` (depois de "continuar") → nunca `PAUSE_AT_MAX`;
    - `count == max` com métricas completas → `COMPLETE`.
  - **Verificação:** `ctest` verde.
- [x] 3. **Regra do `f` e texto das métricas incompletas** (R7, R9).
  `canFinishManually(count)` exige pelo menos 20. `incompleteMetrics(scores)` devolve a lista
  "Skew 72%, Size 90%" a partir dos quatro scores.
  - Testes primeiro: 19 recusa e 20 aceita; todos os scores em 1 dá lista vazia; dois incompletos
    aparecem na ordem X, Y, Size, Skew, com a porcentagem arredondada.
  - **Verificação:** `ctest` verde.

### Fase B — ferramenta

- [x] 4. **Flags** (R1–R3, R11). No `Args` do `stereo_calibration/src/main.cpp`: `--min_samples` e
  `--max_samples` lidos com `parseSampleCount`, mais o registro de que foram passados. A validação
  com `validateSampleLimits` acontece **antes de abrir a câmera**, com saída 1 em caso de erro.
  Também:
  - `--help` atualizado e os limites impressos no início;
  - o `CalibrationChecker` construído com o mínimo da flag e o máximo ilimitado (D1);
  - `min_samples / 4` da cobertura por quadrante passa a usar o mínimo da flag;
  - painel com "[min. N, max. M]" ou "no max";
  - com `--use_stored_values`, aviso de que as flags não têm efeito.
  - **Verificação:** `--help`; os casos de erro de R2 com `echo $?` = 1, sem abrir a câmera; o aviso
    no `--use_stored_values`; a regressão do xadrez idêntica ao `baseline.yml`.
- [x] 5. **Pausa, tecla `f` e registro final** (D3; R4–R9). No laço:
  - depois de cada amostra aceita, `collectionStep` decide:
    - `COMPLETE` termina como hoje;
    - `PAUSE_AT_MAX` liga o estado de pausa;
  - **na pausa**, a imagem ao vivo continua e um aviso sobreposto lista `incompleteMetrics` e as
    teclas. A captura fica desabilitada; `s` ou espaço retoma e levanta o máximo, `f` calibra e `q`
    ou ESC aborta;
  - **fora da pausa**, `f` encerra se `canFinishManually`, ou mostra quantas amostras faltam;
  - a instrução na tela menciona `f`;
  - ao sair do laço, uma linha "Collection ended: <motivo>" com o número de amostras e os quatro
    scores.
  - **Verificação:** build sem warnings; a regressão do xadrez idêntica. O comportamento ao vivo é
    verificado nas etapas 6 e 7.

### Fase C — validação com câmera e acabamento

- [ ] 6. **Validação ao vivo, com o usuário** (critérios de aceitação). Com AprilGrid:
  1. `--max_samples 26`: a coleta pausa na 26ª amostra com métricas incompletas; `s` retoma sem
     capturar; `f` calibra; o `calib.log` registra o motivo.
  2. `f` com menos de 20 amostras mostra a mensagem; com 20 ou mais calibra, com o aviso.
  3. `--max_samples 0`: passa de 35 amostras.
  4. Sem flags, numa coleta que complete as métricas: termina sozinha.
  - **Verificação:** cada item observado na janela e no `calib.log`.
- [ ] 7. **Xadrez ao vivo** (critério "funciona com xadrez e AprilGrid"). Uma coleta curta com o
  tabuleiro de 23,8 mm, usando `f` assim que houver 20 amostras.
  - **Verificação:** calibra e o log registra "manual".
- [x] 8. **README** (R12). Na seção da calibração estéreo: as flags no bloco de uso, a tecla `f`, a
  pausa no máximo e um exemplo com `--max_samples 0`.
  - **Verificação:** os exemplos rodam como escritos.
- [ ] 9. **Fechamento.** Build limpo com e sem `BUILD_TESTS`; `ctest` verde; regressão do xadrez
  idêntica; critérios de aceitação da spec conferidos um a um.

## Riscos

| Risco | Efeito | Mitigação |
|---|---|---|
| Mudar o laço da janela quebra a captura do xadrez | regressão no fluxo atual | lógica nova em funções testadas (D2); xadrez ao vivo na etapa 7; regressão offline depois de 4, 5 e 9 |
| `f` pressionado por engano encerra a coleta | calibração com poucas amostras | exige 20 amostras; com métricas incompletas o aviso fica registrado no log |
| `--max_samples 0` e métricas inalcançáveis | coleta sem fim | o `f` sempre encerra; o painel mostra o score de cada métrica para o usuário decidir |

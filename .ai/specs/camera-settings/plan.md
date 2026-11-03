# Plano — Controles de captura fixos durante a calibração

> **Como implementar**, em micro-tarefas verificáveis. Marque `[x]` ao concluir. O porquê das
> escolhas está em [`decisions.md`](decisions.md) (Dn); os requisitos em [`spec.md`](spec.md) (Rn).

## Pré-requisitos

- Branch `camera-settings`, criada a partir da `aprilgrid-target`.
- Container `isaac-ros-dev`, entrada como `admin`, e o `admin` no grupo `zed` para carregar o ZED
  SDK. Se o container tiver sido recriado: `docker exec --user root isaac-ros-dev usermod -aG zed admin`.
- A linha de base de regressão do xadrez em `calib_runs/regress_chess/`.
- `calib_runs/tools/read_video_settings` para conferir o estado da câmera antes e depois de cada
  execução.
- ZED 2i conectada para as etapas 4 a 8.

## Etapas

### Fase A — leitura do arquivo (TDD, sem câmera)

- [x] 1. **Leitura e nomes** (D1; R2, R4). `common/camera_settings/camera_settings.hpp/.cpp`:
  `loadCameraSettings(path, settings, error)` devolve uma lista de `{nome, valor}`, usando a lista
  dos nomes de `sl::VIDEO_SETTINGS` como strings. Incluir em `tests/CMakeLists.txt`.
  - Testes primeiro:
    - arquivo válido com `EXPOSURE`, `GAIN` e `SHARPNESS` lê os três;
    - nome desconhecido recusado com o nome na mensagem;
    - valor não inteiro recusado;
    - arquivo inexistente ou malformado recusado;
    - arquivo vazio aceito como lista vazia;
    - `SCENE_ILLUMINANCE` recusado como só de leitura;
    - `AEC_AGC_ROI` e `LED_STATUS` recusados como fora de escopo.
  - **Verificação:** `ctest` verde.
- [x] 2. **Regras fixas e ordem** (D2, D3; R3). Na mesma leitura:
  - `AEC_AGC: 1` com `EXPOSURE` ou `GAIN` é recusado;
  - `WHITEBALANCE_AUTO: 1` com `WHITEBALANCE_TEMPERATURE` é recusado;
  - `WHITEBALANCE_TEMPERATURE` fora do passo de 100 é recusado;
  - a lista sai ordenada: valores primeiro, modos por último.
  - Testes primeiro: cada conflito e o passo (4750 recusado, 4700 aceito); `AEC_AGC: 0` sozinho
    aceito; a ordem de saída de um arquivo com `AEC_AGC: 0` antes de `EXPOSURE`.
  - **Verificação:** `ctest` verde.

### Fase B — aplicação na câmera (ZED SDK)

- [x] 3. **Camada do SDK** (D1–D5). `common/camera_settings/zed_camera_settings.hpp/.cpp`, com:
  - `snapshot(camera)`: todos os controles suportados;
  - `validateOnCamera(camera, settings, error)`: suporte e faixa;
  - `apply(camera, settings, error)`: aplica, faz alguns `grab()` e lê de volta;
  - `print(camera, header)`: o valor de cada controle suportado;
  - `restore(camera, snapshot)`: devolve na ordem de D3;
  - um objeto que restaura no destrutor;
  - o tratador de SIGINT, que liga uma `std::atomic<bool>`.

  Incluir nos dois `CMakeLists.txt` das ferramentas.
  - **Verificação:** build sem warnings; `ctest` verde, já que a camada do SDK não entra nos testes.
- [x] 4. **`zed_stereo_checker`** (R1, R4–R8, R12 não se aplica). Flag `--camera_settings` e
  `--help`. Leitura do arquivo antes de abrir a câmera. Depois de abrir: `snapshot`,
  `validateOnCamera`, `apply` e `print`; **sem o arquivo**, só `print` (R7). O laço trata a flag de
  SIGINT como `q`. Restauração na saída.
  - **Verificação (câmera):** com `read_video_settings` antes e depois:
    - sem o arquivo, nada muda e os valores aparecem no início;
    - com o exemplo da etapa 6, os valores são aplicados e impressos, e o `read_video_settings`
      depois de sair com `q` mostra os valores de antes;
    - Ctrl+C também restaura.
- [x] 5. **`zed_stereo_calibration`** (R1, R4–R8, R12). O mesmo da etapa 4, e mais:
  - restauração em todas as saídas depois de abrir a câmera: `q`, o erro da pasta das imagens, o
    fim da coleta antes do `close()` e a flag de SIGINT;
  - uma segunda impressão ao fim da coleta, para conferir R6;
  - com `--use_stored_values`, aviso de que `--camera_settings` não tem efeito e de que os controles
    da captura estão no `calib.log` dela (R12).
  - **Verificação (câmera):**
    - uma coleta curta com o exemplo, encerrada com `q`, restaura;
    - os valores do fim da coleta são iguais aos do início;
    - os casos de erro de R4 (`EXPOSURE_TIME`, `BRIGHTNESS: 9`, `WHITEBALANCE_TEMPERATURE: 4750`,
      `AEC_AGC: 1` com `EXPOSURE`) encerram com código 1, antes da coleta e sem mudar a câmera;
    - o aviso de R12 aparece no `--use_stored_values`;
    - a regressão do xadrez é idêntica.

### Fase C — exemplo, validação e acabamento

- [x] 6. **Arquivo de exemplo** (D6; R9). `config/camera_settings_zed2i.yaml`:
  - os valores da tabela da spec, com `EXPOSURE` 50, `GAIN` 1 e `WHITEBALANCE_TEMPERATURE` 4700 em
    manual;
  - um comentário por controle com a faixa e o efeito na calibração (por exemplo, `SHARPNESS` mexe
    nas bordas usadas para achar os cantos e `EXPOSURE` alta causa borrão de movimento);
  - os controles da ZED X comentados.
  - **Verificação:** a leitura da etapa 1 aceita o arquivo, e as ferramentas o aplicam na ZED 2i
    sem erro.
- [x] 7. **Calibração completa com controles fixos, com o usuário** (critérios de aceitação). Uma
  calibração AprilGrid com `--camera_settings config/camera_settings_zed2i.yaml`, seguindo o
  roteiro de movimentos.
  - **Verificação:**
    - o `calib.log` traz os controles no início e no fim, iguais;
    - o resultado passa nos critérios da spec `aprilgrid-target` (RMS < 0,5 px, fx/fy e baseline a
      ≤ 0,5 % da fábrica);
    - depois de sair, a câmera volta ao estado de antes.
- [x] 8. **Checker com o mesmo arquivo.** Rodar o checker com o arquivo e com a calibração da etapa 7.
  - **Verificação:** os erros aparecem normalmente e o log mostra os controles.
- [x] 9. **README** (R10). Seção "Camera settings":
  - a opção e o formato, apontando para o exemplo;
  - a impressão no início, com ou sem o arquivo;
  - a restauração ao sair;
  - a recomendação de usar o mesmo arquivo na calibração e no checker;
  - o comportamento no `--use_stored_values`.
  - **Verificação:** os exemplos rodam como escritos.
- [x] 10. **Fechamento.** Build limpo com e sem `BUILD_TESTS`; `ctest` verde; regressão do xadrez
  idêntica; critérios de aceitação da spec conferidos um a um.

## Riscos

| Risco | Efeito | Mitigação |
|---|---|---|
| Algum controle só vale depois de alguns quadros | erro falso na leitura de volta | `grab()` antes de ler (D4); a mensagem mostra o pedido e o lido |
| A restauração pula uma saída | a câmera fica em manual para o próximo programa | ponto único de restauração e destrutor (D5); `read_video_settings` antes e depois em cada verificação |
| A ordem dos acoplamentos do SDK é diferente do esperado | a restauração deixa `AEC_AGC` errado | a ordem de D3 e a verificação com `read_video_settings` depois de cada caso |
| Exposição manual errada para a luz da bancada | imagens escuras ou borradas, menos tags | o exemplo é ponto de partida; o checker mostra o efeito antes de calibrar; a escolha dos valores ótimos é não-objetivo |
| Mudar o laço quebra o xadrez ou o AprilGrid | regressão | regressão offline do xadrez depois das etapas 5 e 10; calibração AprilGrid completa na etapa 7 |

## Registro da validação (2026-11-03)

Execução em `calib_runs/2026-11-03_camera_settings/`, com o arquivo de exemplo sem alterações
(`EXPOSURE 50`, `GAIN 1`, `AEC_AGC 0`, `WHITEBALANCE_TEMPERATURE 4700`, `WHITEBALANCE_AUTO 0`).

- **Ferramenta: aprovada.**
  - Os controles foram aplicados e impressos nos três logs (checker de ajuste, calibração e checker
    final).
  - Os valores do início e do fim da coleta são iguais.
  - Depois de sair com `q`, a câmera voltou ao estado anterior (`state_before.txt` contra
    `state_after.txt`). Só `GAIN` e `EXPOSURE` diferem, porque o automático os move.
  - A coleta terminou pelas métricas, com 25 amostras e uma mediana de 27 tags por vista.
  - Antes, sem câmera ou com Ctrl+C num display virtual, já tinham sido verificados os casos de erro,
    o modo sem arquivo, o aviso no `--use_stored_values`, a restauração por Ctrl+C e a regressão do
    xadrez.
- **Calibração:**
  - RMS L / R / Stereo de 0,305 / 0,310 / 0,311 px;
  - fx L / R a +0,31 % / +0,38 % da fábrica;
  - baseline de 120,22 mm (+0,20 %);
  - **fy L / R a +0,59 % / +0,65 % da fábrica, acima do limite de 0,5 % da spec
    `aprilgrid-target`.**

  Na validação cruzada a calibração é tão boa quanto a AprilGrid v2: Stereo de 0,322 px contra
  0,319 px nas imagens da v2.
- **O fy fora do limite não é atribuído a esta feature.** Todas as calibrações AprilGrid (v2,
  acrílico e esta) têm fy/fx de +0,2 a +0,3 %, contra +0,005 % na fábrica e +0,065 % no xadrez. As
  causas prováveis são a placa (escala vertical diferente da horizontal, ~1,4 mm em 572 mm) ou o
  conjunto de imagens capturadas. Fica para investigação fora desta spec, medindo as distâncias
  horizontal e vertical de 5 passos na placa.
- O usuário não salvou prints do checker final, e os erros dele não ficam no log.

Spec dada como concluída pelo usuário em 2026-11-03.

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

- [ ] 1. **Leitura e nomes** (D1; R2, R4). `common/camera_settings/camera_settings.hpp/.cpp`:
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
- [ ] 2. **Regras fixas e ordem** (D2, D3; R3). Na mesma leitura:
  - `AEC_AGC: 1` com `EXPOSURE` ou `GAIN` é recusado;
  - `WHITEBALANCE_AUTO: 1` com `WHITEBALANCE_TEMPERATURE` é recusado;
  - `WHITEBALANCE_TEMPERATURE` fora do passo de 100 é recusado;
  - a lista sai ordenada: valores primeiro, modos por último.
  - Testes primeiro: cada conflito e o passo (4750 recusado, 4700 aceito); `AEC_AGC: 0` sozinho
    aceito; a ordem de saída de um arquivo com `AEC_AGC: 0` antes de `EXPOSURE`.
  - **Verificação:** `ctest` verde.

### Fase B — aplicação na câmera (ZED SDK)

- [ ] 3. **Camada do SDK** (D1–D5). `common/camera_settings/zed_camera_settings.hpp/.cpp`, com:
  - `snapshot(camera)`: todos os controles suportados;
  - `validateOnCamera(camera, settings, error)`: suporte e faixa;
  - `apply(camera, settings, error)`: aplica, faz alguns `grab()` e lê de volta;
  - `print(camera, header)`: o valor de cada controle suportado;
  - `restore(camera, snapshot)`: devolve na ordem de D3;
  - um objeto que restaura no destrutor;
  - o tratador de SIGINT, que liga uma `std::atomic<bool>`.

  Incluir nos dois `CMakeLists.txt` das ferramentas.
  - **Verificação:** build sem warnings; `ctest` verde, já que a camada do SDK não entra nos testes.
- [ ] 4. **`zed_stereo_checker`** (R1, R4–R8, R12 não se aplica). Flag `--camera_settings` e
  `--help`. Leitura do arquivo antes de abrir a câmera. Depois de abrir: `snapshot`,
  `validateOnCamera`, `apply` e `print`; **sem o arquivo**, só `print` (R7). O laço trata a flag de
  SIGINT como `q`. Restauração na saída.
  - **Verificação (câmera):** com `read_video_settings` antes e depois:
    - sem o arquivo, nada muda e os valores aparecem no início;
    - com o exemplo da etapa 6, os valores são aplicados e impressos, e o `read_video_settings`
      depois de sair com `q` mostra os valores de antes;
    - Ctrl+C também restaura.
- [ ] 5. **`zed_stereo_calibration`** (R1, R4–R8, R12). O mesmo da etapa 4, e mais:
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

- [ ] 6. **Arquivo de exemplo** (D6; R9). `config/camera_settings_zed2i.yaml`:
  - os valores da tabela da spec, com `EXPOSURE` 50, `GAIN` 1 e `WHITEBALANCE_TEMPERATURE` 4700 em
    manual;
  - um comentário por controle com a faixa e o efeito na calibração (por exemplo, `SHARPNESS` mexe
    nas bordas usadas para achar os cantos e `EXPOSURE` alta causa borrão de movimento);
  - os controles da ZED X comentados.
  - **Verificação:** a leitura da etapa 1 aceita o arquivo, e as ferramentas o aplicam na ZED 2i
    sem erro.
- [ ] 7. **Calibração completa com controles fixos, com o usuário** (critérios de aceitação). Uma
  calibração AprilGrid com `--camera_settings config/camera_settings_zed2i.yaml`, seguindo o
  roteiro de movimentos.
  - **Verificação:**
    - o `calib.log` traz os controles no início e no fim, iguais;
    - o resultado passa nos critérios da spec `aprilgrid-target` (RMS < 0,5 px, fx/fy e baseline a
      ≤ 0,5 % da fábrica);
    - depois de sair, a câmera volta ao estado de antes.
- [ ] 8. **Checker com o mesmo arquivo.** Rodar o checker com o arquivo e com a calibração da etapa 7.
  - **Verificação:** os erros aparecem normalmente e o log mostra os controles.
- [ ] 9. **README** (R10). Seção "Camera settings":
  - a opção e o formato, apontando para o exemplo;
  - a impressão no início, com ou sem o arquivo;
  - a restauração ao sair;
  - a recomendação de usar o mesmo arquivo na calibração e no checker;
  - o comportamento no `--use_stored_values`.
  - **Verificação:** os exemplos rodam como escritos.
- [ ] 10. **Fechamento.** Build limpo com e sem `BUILD_TESTS`; `ctest` verde; regressão do xadrez
  idêntica; critérios de aceitação da spec conferidos um a um.

## Riscos

| Risco | Efeito | Mitigação |
|---|---|---|
| Algum controle só vale depois de alguns quadros | erro falso na leitura de volta | `grab()` antes de ler (D4); a mensagem mostra o pedido e o lido |
| A restauração pula uma saída | a câmera fica em manual para o próximo programa | ponto único de restauração e destrutor (D5); `read_video_settings` antes e depois em cada verificação |
| A ordem dos acoplamentos do SDK é diferente do esperado | a restauração deixa `AEC_AGC` errado | a ordem de D3 e a verificação com `read_video_settings` depois de cada caso |
| Exposição manual errada para a luz da bancada | imagens escuras ou borradas, menos tags | o exemplo é ponto de partida; o checker mostra o efeito antes de calibrar; a escolha dos valores ótimos é não-objetivo |
| Mudar o laço quebra o xadrez ou o AprilGrid | regressão | regressão offline do xadrez depois das etapas 5 e 10; calibração AprilGrid completa na etapa 7 |

# Bancada — ambiente e armadilhas

Notas para rodar as ferramentas deste repositório no container `isaac-ros-dev` (workspace
`isaac_ros`, ZED 2i S/N 35180697). Resultados e decisões ficam em `.ai/specs/`; os dados das
rodadas, em `ros2_ws/dev/calib_runs/<data>_<o_que>/`, fora deste repositório.

## Permissões

- **O ZED SDK fica ilegível para o usuário `admin`.** `/usr/local/zed` é `root:zed 770`, e o
  entrypoint do container não põe o `admin` no grupo `zed`. Qualquer binário da ZED (estas
  ferramentas e o `zed-ros2-wrapper`) falha com
  `libsl_zed.so: cannot open shared object file`, o que parece instalação quebrada. Correção para o
  container em execução:
  `docker exec --user root isaac-ros-dev usermod -aG zed admin`. Ela some quando o container é
  recriado. A correção permanente seria no `docker/scripts/entrypoint_additions/` do `isaac_ros`,
  e ainda não foi feita.
- **A IMU e os sensores da ZED 2i precisam de acesso ao HID, e o host não tem as regras udev da
  Stereolabs.** A câmera abre, mas o SDK registra `[ZED][MCU] Permissions denied` e
  `Sensors could not be initialized`. Sem IMU não há `left_cam_imu_transform` nem TF `imu_link`, e o
  `LED_STATUS` não pode ser lido. O SDK abre o HID via libusb, então **dois** nós precisam de
  escrita:
  - o `hidraw` cujo `/sys/class/hidraw/*/device/uevent` mostra `2B03:F881`;
  - o `/dev/bus/usb/<bus>/<dev>` correspondente, visto no `lsusb`.

  Correção rápida: `chmod 666` nos dois, como root no container (o `/dev` é o do host). Ela some ao
  desconectar a câmera. A correção permanente seria uma regra udev no host para `idVendor 2b03`, que
  ainda não foi instalada.

## Rodando as ferramentas

- **Sem `--session`, a captura ao vivo apaga o `zed-images/` do diretório atual.** Comece cada
  rodada numa pasta nova em `calib_runs/`. O `--use_stored_values` só lê, não apaga.
- **Com `--session`, nada é apagado.** A captura vai para
  `calib_runs/dados/<condicao>/captura_<XX>/`, e uma pasta já usada é recusada antes de abrir a
  câmera: troque o número da captura no arquivo de sessão. Testes ficam fora de `dados/`, em
  `calib_runs/testes_persistencia/`.
- **Calibração de fábrica (F):** `zed_export_factory --output_root <dados> --verify
  --expect_resolution 1280x720` grava `dados/fabrica/SN<serial>/`. Rode uma vez por câmera; a câmera
  do robô tem outro serial e outra F. A distorção que o SDK entrega é a do modelo racional, a seção
  `[LEFT_DISTO]`/`[RIGHT_DISTO]` do `.conf`, e não o k1/k2 de `[LEFT_CAM_HD]`.
- **Exportar as detecções de uma captura antiga** sem tocar na pasta original: numa pasta de
  trabalho, faça um link `zed-images` para o `zed-images/` original e rode
  `--use_stored_values --detections_out detections.json` ali. O YAML do recálculo é gravado na
  pasta de trabalho.
- **Testar com a câmera sem abrir janela na tela do usuário:** rode a ferramenta num `Xvfb :98`
  próprio e encerre com `timeout -s INT`. O Ctrl+C restaura os controles de captura
  (`--camera_settings`). Encerre esse Xvfb pelo PID: nunca `killall Xvfb`, porque o container do
  Orin usa `pid: host`.
- **Só execute um binário depois de confirmar que a compilação passou.** Um build quebrado deixa o
  binário antigo no lugar. Ele ignora as flags novas e abre a câmera e a janela, como aconteceu em
  2026-11-03 com os testes de erro do `--min_samples`.
- **Regressão do xadrez:** `calib_runs/regress_chess/` tem `baseline.log` e `baseline.yml`. Depois
  de qualquer mudança em `stereo_calibration/`, rode
  `zed_stereo_calibration --use_stored_values --square_size 23.8` nessa pasta e compare com `diff`
  e `cmp`.
- **O estado dos controles da câmera** pode ser lido sem alterar nada com
  `calib_runs/tools/read_video_settings`.
- **Testar uma captura com sessão sem tocar no teclado:**
  `calib_runs/tools/bench_session_capture.sh <condicao> <captura> <toques>` roda a ferramenta num
  `Xvfb :98` próprio, aperta `s` o número de vezes pedido (com `calib_runs/tools/sendkey`, via XTest)
  e depois `q`. `calib_runs/tools/check_detections <captura> <target.yaml>` detecta os PNGs de novo
  e compara com o `detections.json`. Com o alvo parado, só o primeiro par passa no filtro de
  dissimilaridade.
- **O `FILE(GLOB ...)` do CMake só lista os arquivos na configuração.** Depois de criar um `.cpp`
  novo, rode `cmake ..` antes do `make`, senão o link falha com "undefined reference".
- **Testes unitários:** `cmake -S tests -B build/unit_tests && make -C build/unit_tests &&
  build/unit_tests/aprilgrid_tests`, no container, como `admin`.
- **Erros do checker sem janela:** `calib_runs/tools/checker_errors <target.yaml> <quadros>
  [calib.yml|internal] [pasta_zed-images/]` calcula os mesmos erros do `zed_stereo_checker`
  (esquerda, direita, estéreo), ao vivo ou nos pares gravados, e imprime a mediana e o IQR.
- **Recalcular uma captura de sessão sem a câmera:** `zed_stereo_calibration --target_yaml
  <target.yaml> --recalibrate <captura_XX>`. Para testar com uma rodada antiga,
  `calib_runs/tools/make_session_capture.py` monta uma captura de sessão a partir do `zed-images/` e
  do `detections.json` exportado, copiando as imagens sem tocar na original.
  `calib_runs/tools/check_report_rms.py <captura>` recalcula em Python os RMS do relatório.
- **O checker abre com um arquivo de calibração?** `calib_runs/tools/bench_checker_load.sh <calib.yml>`
  roda o `zed_stereo_checker --calib_opencv` num `Xvfb :98` por alguns segundos. Não precisa do alvo.

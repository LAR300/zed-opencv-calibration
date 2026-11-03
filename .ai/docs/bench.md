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

- **A captura ao vivo apaga o `zed-images/` do diretório atual.** Comece cada rodada numa pasta
  nova em `calib_runs/`. O `--use_stored_values` só lê, não apaga.
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

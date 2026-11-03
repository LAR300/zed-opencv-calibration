# Spec — Acesso ao ZED SDK e ao HID da câmera na bancada

> **O quê, por quê e condições de implementação.** Origem: [`auditoria.md`](../../auditoria.md)
> §1.2 e §6 (risco 12); [`.ai/docs/bench.md`](../../docs/bench.md) § Permissões. Pré-requisito da
> temperatura em [`persistencia-captura`](../persistencia-captura/spec.md) (R9) e de
> [`aquecimento-termico`](../aquecimento-termico/spec.md).

## Problema

Duas permissões quebram a bancada a cada container novo ou a cada vez que a câmera é reconectada:

1. **O ZED SDK é ilegível para o `admin`.** `/usr/local/zed` é `root:zed 770` e o `admin` não está
   no grupo `zed`. Todo binário falha com `libsl_zed.so: cannot open shared object file`. A
   correção manual (`usermod -aG zed admin`) some quando o container é recriado.
2. **O HID da ZED 2i não tem permissão.** O SDK registra `[ZED][MCU] Permissions denied` e não
   inicializa os sensores. Sem eles:
   - não há temperatura, e o `meta.json` exige temperatura (R1 do brief);
   - não há IMU nem `LED_STATUS`.

   A correção manual (`chmod 666` no `hidraw` e no `/dev/bus/usb/...`) some ao desconectar.

## Requisitos

- **R1.** Uma regra udev para o host, com `idVendor 2b03` (Stereolabs), dá acesso ao `hidraw` e ao
  nó USB da câmera, de forma persistente entre reconexões.
- **R2.** Um script **neste repositório** instala a regra no host, de forma idempotente, com
  `--dry-run`, e recarrega o udev.
- **R3.** Um script neste repositório põe o `admin` no grupo `zed` do container em execução e
  confere se `libsl_zed.so` abre, para usar depois de recriar o container.
- **R4.** Uma verificação (`--check`) informa, sem alterar nada:
  - se o `admin` lê o SDK;
  - se o HID está acessível;
  - se `getSensorsData` devolve a temperatura `ONBOARD_LEFT`.
- **R5.** O `.ai/docs/bench.md` passa a apontar para os scripts, em vez dos comandos manuais.

## Condições de implementação

- **Tudo neste repositório**, por decisão do usuário (trabalhar sempre no `zed-opencv-calibration`):
  - `scripts/bench/install_zed_udev.sh`;
  - `scripts/bench/fix_zed_group.sh`;
  - `scripts/bench/check_zed_access.sh`;
  - `config/udev/99-stereolabs-zed.rules`.
- **O conserto definitivo do grupo `zed`** seria no `entrypoint_additions/` do repositório
  `isaac_ros`. Ele fica **registrado como recomendação**, não implementado aqui.
- **O modo de permissão da regra** (grupo `plugdev` contra `0666`) segue o que o host já usa para a
  RealSense (`docker/udev_rules/99-realsense-libusb-custom.rules` no `isaac_ros`).

## Não-objetivos

- Alterar o repositório `isaac_ros`.
- Outros sensores do robô.

## Critérios de aceitação

- [ ] Depois de instalar a regra e reconectar a câmera, `check_zed_access.sh --check` mostra o HID
      acessível e uma temperatura `ONBOARD_LEFT`, sem `chmod` manual.
- [ ] Depois de recriar o container, `fix_zed_group.sh` restaura o acesso ao SDK, e o `--check`
      confirma.
- [ ] Rodar o instalador duas vezes não duplica nada, e `--dry-run` não altera o host.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Instalar a regra udev no host do notebook** exige `sudo` fora do
  container. Confirmar se é aceitável.

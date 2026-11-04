# Plano — Acesso ao ZED SDK e ao HID

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- Acesso `sudo` ao host do notebook, aprovado pelo usuário.

## Etapas

- [x] 1. **Verificação** (R4). `scripts/bench/check_zed_access.sh --check` (host) e o binário
  `zed_access_check` (`access_check/`; abre como a calibração e lê as três temperaturas; códigos
  0, 2 sem câmera, 3 sem sensores).
  - **Desvio do critério:** o estado atual já não tinha os dois problemas (o `admin` estava no
    grupo `zed` por um `usermod` anterior, e o nó do HID estava `0666`). Em 2026-11-04 o `--check`
    passou no SDK, no HID e nas temperaturas e reprovou a falta da regra udev. O caso "sem
    correções" fica para depois de recriar o container ou de reconectar a câmera
    (pendências de bancada).
  - **Achados:** a interface de vídeo (`f880`, `0664`) não precisa de escrita, porque passa pelo
    UVC; e o `hidraw` some depois que o SDK abre a câmera. O check trata os dois casos.

  Especificação original da etapa: `check_zed_access.sh --check`: SDK legível pelo `admin`, nós HID e USB
  da `2b03` com permissão, e uma leitura de temperatura por um binário mínimo ou pela ferramenta de
  bancada.
  - **Pronto quando:** no estado atual, sem correções, o script aponta os dois problemas.
- [ ] 2. **Regra udev e instalador** (R1, R2). `config/udev/99-stereolabs-zed.rules` e
  `scripts/bench/install_zed_udev.sh` (`--dry-run` verificado). **Falta** a instalação real, que
  pede `sudo` no host (pendências de bancada).
  - **Pronto quando:**
    - `--dry-run` mostra o que faria;
    - a instalação real, seguida de reconexão da câmera, faz o `--check` passar no HID;
    - uma segunda execução não altera nada.
- [ ] 3. **Grupo `zed` no container** (R3). `scripts/bench/fix_zed_group.sh` (idempotente,
  `--dry-run`); confere o carregamento da `libsl_zed.so` como `admin`. **Falta** o teste depois
  de recriar o container.
  - **Pronto quando:** depois de `docker restart` (ou de recriar o container), o script restaura o
    acesso e o `--check` passa no SDK.
- [x] 4. **Documentação** (R5). `.ai/docs/bench.md` aponta para os scripts e traz a recomendação
  do `entrypoint_additions/` do `isaac_ros`. `.ai/docs/bench.md` atualizado, com a recomendação do conserto
  definitivo no `isaac_ros`.
  - **Pronto quando:** o `bench.md` não tem mais `chmod` nem `usermod` manuais.

## Riscos

- **Uma regra `0666` abre o dispositivo a qualquer usuário do host.** Preferir um grupo, se o host
  já tiver um em uso para dispositivos USB.

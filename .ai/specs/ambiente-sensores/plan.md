# Plano — Acesso ao ZED SDK e ao HID

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- Acesso `sudo` ao host do notebook, aprovado pelo usuário.

## Etapas

- [ ] 1. **Verificação** (R4). `check_zed_access.sh --check`: SDK legível pelo `admin`, nós HID e USB
  da `2b03` com permissão, e uma leitura de temperatura por um binário mínimo ou pela ferramenta de
  bancada.
  - **Pronto quando:** no estado atual, sem correções, o script aponta os dois problemas.
- [ ] 2. **Regra udev e instalador** (R1, R2).
  - **Pronto quando:**
    - `--dry-run` mostra o que faria;
    - a instalação real, seguida de reconexão da câmera, faz o `--check` passar no HID;
    - uma segunda execução não altera nada.
- [ ] 3. **Grupo `zed` no container** (R3).
  - **Pronto quando:** depois de `docker restart` (ou de recriar o container), o script restaura o
    acesso e o `--check` passa no SDK.
- [ ] 4. **Documentação** (R5). `.ai/docs/bench.md` atualizado, com a recomendação do conserto
  definitivo no `isaac_ros`.
  - **Pronto quando:** o `bench.md` não tem mais `chmod` nem `usermod` manuais.

## Riscos

- **Uma regra `0666` abre o dispositivo a qualquer usuário do host.** Preferir um grupo, se o host
  já tiver um em uso para dispositivos USB.

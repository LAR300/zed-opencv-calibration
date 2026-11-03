# Spec — Piloto das métricas nas capturas existentes

> **O quê, por quê e condições de implementação.** Origem: [`backlog.md`](../../product/backlog.md)
> (piloto antes do roteiro); [`auditoria.md`](../../auditoria.md) §6 (risco 11);
> `aprilgrid-target/validation.md` Parte 4 (fy/fx). Depende de
> [`persistencia-captura`](../persistencia-captura/spec.md) R10,
> [`saida-calibracao`](../saida-calibracao/spec.md), [`exportar-fabrica`](../exportar-fabrica/spec.md),
> [`nucleo-offline`](../nucleo-offline/spec.md), [`metrica-ajuste`](../metrica-ajuste/spec.md) e
> [`erro-mapeamento`](../erro-mapeamento/spec.md).

## Problema

Antes de investir em 10 capturas por condição, é preciso saber três coisas:
- **as métricas funcionam em dados reais;**
- **o RMS de ~0,31 px e o fy/fx +0,2–0,3 % do AprilGrid são viés,** que as capturas da condição A
  herdariam;
- **qual variante de calibração** os experimentos devem usar.

As capturas que já existem respondem a isso sem coleta nova.

## Contexto

Capturas disponíveis em `ros2_ws/dev/calib_runs/` (só leitura):

| Captura | Alvo | Pares | Observação |
|---|---|---:|---|
| `2026-10-16_ar_9x6` | xadrez 9×6, 23,8 mm | 35 | RMS 0,24–0,25 px; fy/fx +0,065 % |
| `2026-10-20_ar_aprilgrid_v2` | AprilGrid | 29 | fy/fx +0,205 % |
| `2026-10-23_acrilico_ar_aprilgrid` | AprilGrid, com acrílico | 35 | fy/fx +0,302 % |
| `2026-11-03_camera_settings` | AprilGrid, controles fixos | 25 | fy/fx +0,281 % |

- A F é a da S/N 35180697, com fy/fx +0,005 %.
- O alvo AprilGrid está descrito em `calib_runs/2026-10-20_aprilgrid_checker/target.yaml`.

## Requisitos

- **R1.** As detecções das 4 capturas são exportadas pelo C++ (`--detections_out`) para
  `calib_runs/piloto/<captura>/detections.json`, sem tocar as pastas originais. F é exportada com
  `zed_export_factory`.
- **R2.** Para cada captura, o piloto gera quatro variantes de calibração `rational8`:
  - compatível com o C++;
  - mono com todas as tags por câmera + estéreo `joint`;
  - a mesma, com estéreo `fixed`;
  - a mesma, sem rejeição de outliers.
- **R3.** Para cada calibração (as 16, mais F), o piloto roda o M1 (RMS, mapa de resíduos, BR por
  câmera) e o M2 (pares entre capturas AprilGrid e contra F). F é avaliada nas detecções de cada
  captura.
- **R4.** O resultado responde, com números e figuras, a seis perguntas:
  1. **σ̂_d do nosso detector**, no AprilGrid e no xadrez, comparado ao ~0,03 px de Hagemann.
  2. **O BR das calibrações AprilGrid** comparado ao do xadrez. O RMS de ~0,31 px é dominado por
     viés?
  3. **O mapa de resíduos** mostra um padrão vertical ou dependente da região que explique o fy/fx?
  4. **Usar todas as tags por câmera** (contra só as comuns) muda fy/fx, o BR ou a cobertura das
     bordas?
  5. **`joint` × `fixed` e com × sem rejeição:** quanto mudam os parâmetros e o erro de mapeamento
     entre as variantes?
  6. **As três referências "média" do M2** diferem de forma relevante?
- **R5.** O registro vai para `.ai/specs/piloto-dados-existentes/resultado.md`, com tabelas,
  figuras (em `calib_runs/piloto/`) e uma **recomendação** para cada decisão pendente:
  - a variante de calibração dos experimentos;
  - um limiar de BR, se houver;
  - a necessidade de uma feature de correção da detecção antes da condição A.

## Condições de implementação

- **Nenhum arquivo das capturas originais é alterado.** Tudo é gravado em `calib_runs/piloto/`.
- **A configuração de cada análise fica salva junto do resultado** (R12 do núcleo).
- **O piloto não decide nada sozinho:** recomenda, e o usuário aprova.

## Não-objetivos

- Coletar dados novos. O teste com o alvo girado 90° depende do roteiro e entra na condição A.
- Corrigir a detecção: se for preciso, vira feature própria.

## Critérios de aceitação

- [ ] As 4 capturas têm `detections.json` exportado, e o modo compatível reproduz os YAML do C++
      (até 1e-6 relativo) nas capturas AprilGrid.
- [ ] As 16 calibrações e F têm o relatório M1. Os pares AprilGrid e F têm o M2.
- [ ] `resultado.md` responde às seis perguntas de R4, com números, e traz as recomendações de R5.
- [ ] O usuário aprova ou ajusta as recomendações. A decisão é registrada em
      [`experimentos`](../experimentos/spec.md).

## Perguntas em aberto

Nenhuma. As decisões saem do resultado.

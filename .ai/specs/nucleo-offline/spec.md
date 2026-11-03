# Spec — Núcleo offline em Python (`analysis/`)

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R1 (reprocessar com outros modelos), R4 e "Módulos sugeridos: metricas"; e
> [`auditoria.md`](../../auditoria.md) §5 e §6 (riscos 4, 5 e 6).
>
> É a base de todos os módulos offline: [`metrica-ajuste`](../metrica-ajuste/spec.md),
> [`erro-mapeamento`](../erro-mapeamento/spec.md), [`cobertura`](../cobertura/spec.md),
> [`validacao-cruzada`](../validacao-cruzada/spec.md), [`metricas-tarefa`](../metricas-tarefa/spec.md),
> [`analise-estatistica`](../analise-estatistica/spec.md) e
> [`validacao-metrica`](../validacao-metrica/spec.md).

## Problema

As métricas M1–M4 e a estatística R6 precisam de três coisas que a ferramenta C++ não oferece:
1. **ler as capturas salvas;**
2. **recalibrar com outros modelos e variações:** por câmera com todas as tags, com ou sem rejeição
   de outliers, com os intrínsecos fixos ou livres no estéreo;
3. **gerar dados sintéticos com verdade conhecida,** para testar as métricas.

Os scripts descartáveis de `calib_runs/tools/` fazem parte disso, mas:
- **detectam os cantos em Python, sem o refino D11** (risco 6);
- **têm a geometria fixa no código** (`PITCH = 88.0 * 1.3`).

## Contexto

- **Formatos de entrada** em [`persistencia-captura`](../persistencia-captura/spec.md) § Formatos:
  `detections.json`, `meta.json` e o arquivo de sessão.
- **O YAML de calibração** é o do SDK (`opencv_calibration.cpp:455-477`):
  - `D_*` pode ter 5, 8 ou 14 colunas, ou `_FE` com 4;
  - `R` é um vetor de Rodrigues;
  - `T` está em mm.
- **Convenção dos pontos 3D** do alvo (Kalibr): `common/aprilgrid/aprilgrid.cpp:98-119`.
- **O RMS do OpenCV é por canto, com norma 2D** (`calibration.cpp:1868`, `:2443` do 4.5.4;
  auditoria §3.4).
- **O `stereoCalibrateExtended` do 4.5.4** dá o erro por vista, mas não os desvios-padrão nem as
  poses. A incerteza estéreo, quando necessária, sai por bootstrap: Hagemann (2022) mostra que a
  covariância padrão é otimista em dados reais ([`referencias.md`](../../product/referencias.md)
  §A.4).
- **Ambiente:** container `isaac-ros-dev`, com Python 3, NumPy 1.26.4, SciPy 1.15.3,
  OpenCV 4.5.4, Matplotlib 3.5.1, PyYAML e pytest 6.2.5.

## Requisitos

### Leitura e escrita
- **R1.** O pacote lê `detections.json` e `meta.json` (`schema_version: 1`; outra versão é
  recusada com erro) e o arquivo de sessão. Também trata as capturas antigas
  (`pose_id = legacy`) e o alvo `checkerboard`.
- **R2.** O pacote lê e escreve o YAML de calibração no formato do SDK: `D` com 5, 8 ou 14 colunas
  (14 = os 8 racionais mais zeros) e `_FE`. O que ele grava passa no
  `zed_stereo_checker --calib_opencv`.
- **R3.** O pacote calcula os pontos 3D do alvo a partir de `detections.json`, com a mesma
  convenção e a mesma ordem de cantos do C++.

### Calibração offline
- **R4.** Modelos disponíveis: `radtan5` (k1, k2, p1, p2, k3), `rational8` (k1–k6, p1, p2) e
  `fisheye4`.
- **R5.** **Calibração por câmera com todas as tags daquela câmera.** **Calibração estéreo com as
  tags comuns.** O estéreo tem dois modos:
  - `joint`: intrínsecos livres, como o C++;
  - `fixed`: `CALIB_FIX_INTRINSIC` sobre os mono.
- **R6.** Rejeição de outliers configurável: desligada, ou a regra do C++ (por quadro, fator e
  piso).
- **R7.** **Modo de compatibilidade com o C++:** mono só com as tags comuns, `joint`, a regra de
  rejeição do C++ e o prior opcional. Com as mesmas detecções, ele reproduz `calib_cpp_rational8.yml`.
- **R8.** **Cada calibração offline grava `calib_<modelo>[_<variante>].yml` e um relatório JSON**,
  no formato do relatório de [`saida-calibracao`](../saida-calibracao/spec.md) (R3–R6):
  - RMS na convenção `per_corner_2d`;
  - MSE por coordenada;
  - erro por vista;
  - desvios-padrão mono (Extended);
  - vistas removidas;
  - commit do pacote.

### Utilidades comuns
- **R9.** Funções de projeção e resíduos com sinal (x e y separados), por canto, a partir de uma
  calibração e das detecções:
  - com pose livre por vista (`solvePnP`, `IPPE` para alvo plano, refinado com LM);
  - com a pose da esquerda e R, T para a direita.
- **R10.** Bootstrap sobre as vistas: reamostragem com reposição, n configurável e semente fixa,
  para a incerteza dos parâmetros.
- **R11.** **Gerador sintético.** Câmera estéreo com K, D, R e T dados; poses do alvo dadas ou
  sorteadas; ruído gaussiano σ nos cantos; vieses opcionais (alvo não plano com flecha dada,
  escala do alvo errada). Ele produz um `detections.json` válido.
- **R12.** **Configuração de cada análise em YAML,** salva junto com a saída. A execução é
  reproduzível: semente, commit e versões das bibliotecas vão para o relatório.

## Condições de implementação

- **Sem detecção de cantos em Python.** O pacote **não** importa `cv2.aruco` nem chama
  `findChessboardCorners`. Um teste verifica isso. Os cantos vêm sempre do C++ (risco 6; Hagemann,
  armadilha 4).
- **Sem dependência nova;** tudo roda no container.
- **Estrutura:**
  - `analysis/zedcal/`: o pacote, com `io`, `target`, `calib`, `residuals`, `bootstrap`,
    `synthetic` e `config`;
  - `analysis/tests/`: pytest;
  - `analysis/configs/`: exemplos.

  Execução com `python3 -m zedcal.<comando> --config <yaml>`.
- **Unidades:** mm para o alvo e para T; px para a imagem. Todo valor de erro traz a convenção no
  nome do campo, por exemplo `rms_per_corner_px` e `mse_per_coord_px2`.
- **PnP:** `SOLVEPNP_IPPE` para o alvo inteiro e `SOLVEPNP_IPPE_SQUARE` para uma tag isolada;
  depois `solvePnPRefineLM`. A ambiguidade de 2 soluções do PnP planar é resolvida pelo menor
  resíduo (Hagemann, armadilha 5).

## Não-objetivos

- As métricas M1–M4, a cobertura e a estatística: cada uma tem o seu spec.
- Modelos refrativos, que são da etapa na água.
- Interface gráfica.

## Critérios de aceitação

- [ ] O modo de compatibilidade, com as detecções exportadas de
      `2026-11-03_camera_settings`, reproduz o YAML do C++ (K, D, R, T) até 1e-6 relativo.
- [ ] Os pontos 3D das tags 0, 5, 30 e 35 coincidem com os valores de
      `tests/test_geometry.cpp`.
- [ ] Um YAML gravado pelo pacote é carregado pelo `zed_stereo_checker --calib_opencv`.
- [ ] Os dados sintéticos sem ruído, calibrados com o modelo verdadeiro, recuperam K, D, R e T até
      1e-6 relativo, com RMS < 1e-6 px.
- [ ] O teste de "sem detecção em Python" passa.
- [ ] `python3 -m pytest analysis/tests` passa no container.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Modo estéreo das calibrações dos experimentos:** `joint` (como o C++) ou
  `fixed`. Proposta: o piloto calcula os dois e o spec [`experimentos`](../experimentos/spec.md)
  fixa um antes da condição A.

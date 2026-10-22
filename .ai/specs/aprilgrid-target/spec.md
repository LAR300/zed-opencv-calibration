# Spec — Alvo AprilGrid nas ferramentas estéreo

> **O quê e por quê.** Fonte da verdade da intenção. **Sem decisão técnica nem passos de
> implementação** (esses vão em `decisions.md` e `plan.md`).

## Problema

`zed_stereo_calibration` e `zed_stereo_checker` só reconhecem tabuleiro de xadrez. O xadrez só
é detectado quando aparece **inteiro** nas duas imagens. Isso causa três limitações:

- **Bordas da imagem.** A distorção da lente é mais forte nos cantos. Para levar pontos até lá, o
  tabuleiro inteiro precisa caber no quadro. Com vista parcial, metade do alvo pode ficar fora do
  quadro e a borda fica cheia de pontos.
- **Alvo grande.** O AprilGrid da equipe (6x6 tags de 88 mm, ~66 cm de lado) só cabe inteiro na
  imagem da ZED 2i a partir de ~0,5 m. Isso impede amostras próximas e dificulta completar as
  métricas de cobertura.
- **Uso submerso (próxima etapa).** Turbidez, perda de luz e reflexos costumam esconder parte do
  alvo. Exigir o alvo inteiro nas duas câmeras ao mesmo tempo descartaria a maioria dos quadros.

O AprilGrid resolve as três, porque cada tag tem um ID próprio: qualquer subconjunto visível já
fornece correspondências 2D↔3D corretas. A equipe já tem esse alvo, gerado pelo Kalibr e descrito
por um `target.yaml`, e o usa em outras etapas de calibração.

## Contexto

- Repositório: fork LAR300 de `stereolabs/zed-opencv-calibration`, sobre o commit `fcfaeae`. São cinco
  ferramentas, cada uma um projeto CMake independente: calibração e checker, estéreo e mono, mais o
  visualizador de reprojeção. `stereo_calibration` e `monocular_calibration` têm cópias quase
  idênticas de `calibration_checker.cpp` e `opencv_calibration.cpp`. O repositório não tem testes
  automatizados.
- Fluxo atual de `zed_stereo_calibration`:
  - captura ao vivo com o tabuleiro detectado nas duas imagens, filtro de nitidez e checagem de
    variedade;
  - métricas de cobertura X, Y, Size e Skew na tela, com o cálculo disparando sozinho entre 25 e 35
    amostras;
  - recálculo das imagens salvas com `--use_stored_values`;
  - saída `zed_calibration_<sn>.yml`, recusada se algum RMS passar de 0,5 px, se o baseline tiver o
    sinal trocado ou se ele for menor que 30 mm.
- As métricas de cobertura (`CalibrationChecker`) são calculadas a partir dos 4 cantos externos do
  grid **completo** e não funcionam com detecção parcial.
- Alvo da equipe (`target.yaml` do Kalibr): `target_type: 'aprilgrid'`, `tagCols: 6`, `tagRows: 6`,
  `tagSize: 0.088` (m) e `tagSpacing: 0.3` (razão espaço/tag, ou seja, 26,4 mm). Família tag36h11,
  IDs 0–35, impresso pelo Kalibr.
- O container tem OpenCV 4.5.4 com o módulo `aruco` do contrib.
- Já existe uma implementação em Python, no workspace ROS:
  `prj-minirov-petro-contrib-pkgs/petro_data_replayer/petro_data_replayer/calibrate_intrinsics.py`
  (`compute_aprilgrid_object_points`, `detect_aprilgrid`). Ela documenta a ordem de IDs e cantos
  "empiricamente verificada contra o board Kalibr real" e serve de referência.
- Linha de base com xadrez já medida, no ar, com a ZED 2i S/N 35180697 em HD720 ([`validation.md`](validation.md),
  Parte 1). Quadrado de 23,8 mm, 35 amostras.

  | Parâmetro | Fábrica | Xadrez |
  |---|---:|---:|
  | RMS L / R / Estéreo | — | 0,237 / 0,238 / 0,254 px |
  | fx L | 533,82 | 533,04 |
  | fy L | 533,85 | 533,39 |
  | fx R | 533,32 | 532,80 |
  | fy R | 533,44 | 533,02 |
  | Baseline | 119,98 mm | 120,04 mm |

  As imagens dessa execução estão em `../calib_runs/2026-10-16_ar_9x6/zed-images/`.
- Lição dessa execução: o tamanho real impresso diferia do nominal (23,8 contra 25,4 mm). O RMS não
  denunciou o erro; só o baseline e o checker estéreo denunciaram.

## Requisitos

### Seleção do alvo
- **R1.** `zed_stereo_calibration` e `zed_stereo_checker` aceitam `--target_yaml <arquivo>`, que
  aponta para um `target.yaml` no formato do Kalibr. Sem essa opção, as duas ferramentas se
  comportam **exatamente como hoje**, com xadrez e as opções `--h_edges`, `--v_edges` e
  `--square_size`.
- **R2.** As ferramentas leem do arquivo `target_type`, `tagCols`, `tagRows`, `tagSize` (em metros) e
  `tagSpacing` (razão). Arquivo ausente, ilegível, com campo faltando, valor inválido ou
  `target_type` diferente de `aprilgrid` gera **erro claro e encerra** a ferramenta, sem
  calibrar.
- **R3.** Ao iniciar, as ferramentas imprimem as características do alvo lido, no mesmo estilo do
  que já imprimem para o xadrez:
  - número de tags;
  - tamanho da tag e espaçamento em mm;
  - dimensão física total esperada.

  Assim o usuário pode conferir com uma régua se os valores batem com a folha impressa.

### Detecção
- **R4.** Alvos AprilGrid **gerados pelo Kalibr** (família tag36h11) são detectados corretamente.
  Cada tag detectada é associada à sua posição física real no alvo, segundo a convenção de IDs e
  de cantos do Kalibr.
- **R5.** Tags fora do alvo (ID inexistente no grid) ou incompletas na imagem são ignoradas.
- **R6.** A sensibilidade da detecção pode ser ajustada **sem recompilar**, para preparar o uso
  submerso.

### Vista parcial
- **R7.** Uma amostra estéreo vale quando as tags vistas **pelas duas câmeras ao mesmo tempo**
  somam **≥ 12** e ocupam **≥ 2 linhas e ≥ 2 colunas** do alvo. Esses mínimos são o padrão e podem
  ser alterados pela linha de comando.
- **R8.** Uma amostra abaixo do mínimo é recusada com uma mensagem que diz o motivo e quantas tags
  foram vistas, assim como hoje aparecem "alvo ausente em uma câmera", "alvo parecido demais" e
  "imagem borrada".
- **R9.** Na calibração estéreo, cada par de imagens usa só as tags vistas pelas duas câmeras.

### Funcionalidades que se mantêm
- **R10.** Com AprilGrid, `zed_stereo_calibration` mantém todo o fluxo atual:
  - visualização ao vivo das duas imagens com as detecções desenhadas;
  - métricas X, Y, Size e Skew com MIN, MAX, COVERAGE, REQUIRED e SCORE;
  - sobreposição de cobertura;
  - captura com `s` ou espaço, com filtro de nitidez e checagem de variedade;
  - mínimo e máximo de amostras e cálculo automático;
  - imagens salvas;
  - mesmas checagens de qualidade (RMS ≤ 0,5 px, sinal e mínimo do baseline);
  - mesmo arquivo de saída `zed_calibration_<sn>.yml`.
- **R11.** As métricas de cobertura funcionam com vista parcial e refletem só a área efetivamente
  coberta pelas tags detectadas.
- **R12.** As tags detectadas aparecem identificadas na visualização ao vivo, para o usuário
  conferir a orientação e a numeração do alvo.
- **R13.** `--use_stored_values --target_yaml <arquivo>` recalcula a calibração a partir das imagens
  salvas, sem câmera, como já acontece com o xadrez.
- **R14.** `zed_stereo_checker --target_yaml <arquivo>` mostra os erros ao vivo (Left, Right e
  Stereo), com a calibração interna ou com `--calib_opencv`, aceitando vista parcial pelo mesmo
  critério de mínimo de R7.
- **R15.** `--fisheye` e `--virtual` continuam aceitos junto com `--target_yaml`, sem perder o
  funcionamento que têm hoje.

### Código e documentação
- **R16.** O código novo segue o estilo, a organização e as convenções do repositório:
  - C++17;
  - nomes e formato iguais ao código existente;
  - parse de argumentos no padrão atual;
  - mensagens de console em inglês;
  - sem dependência nova além do OpenCV e do ZED SDK.
- **R17.** O `README.md` documenta o uso do AprilGrid nas duas ferramentas, em inglês como o resto
  do arquivo:
  - formato do `target.yaml`;
  - exemplos de comando;
  - mínimo de tags;
  - recomendação de medir o alvo impresso.

## Não-objetivos

- Ferramentas mono (`zed_mono_calibration`, `zed_mono_checker`) e `zed_reprojection_viewer`.
- Outras famílias de tags, ChArUco, o `target.yaml` de xadrez do Kalibr ou outros formatos de alvo.
- Validação submersa. Fica como uso futuro; esta spec só garante R6.
- Validação com `--fisheye` ou `--virtual`. Basta não quebrar (R15).
- Exportar resultados no formato do Kalibr (camchain etc.).
- Mudar qualquer comportamento do modo xadrez.
- Enviar a mudança para o upstream da Stereolabs. Ela vive só no fork LAR300.

## Critérios de aceitação

- [x] **Sem regressão no xadrez.** `zed_stereo_calibration --use_stored_values --square_size 23.8`
      sobre as imagens de `../calib_runs/2026-10-16_ar_9x6/zed-images/` reproduz a linha de base:
      RMS 0,237 / 0,238 / 0,254 px e baseline 120,04 mm, iguais nas casas impressas.
- [x] **Calibração AprilGrid no ar** com a ZED 2i S/N 35180697, em HD720:
  - [x] RMS Left, Right e Stereo < 0,5 px;
  - [x] fx e fy das duas câmeras a **≤ 0,5 %** do resultado do xadrez **e** da fábrica;
  - [x] baseline a **≤ 0,5 %** de 120,04 mm (xadrez) **e** de 119,98 mm (fábrica);
  - [x] cx e cy reportados na comparação, sem limite de aprovação.
- [x] **A vista parcial é aceita.** Pelo menos uma amostra aceita tem parte do alvo fora do quadro, e
      a coleta chega a 100 % nas quatro métricas ou ao máximo de amostras.
- [x] **O mínimo é respeitado.** Uma amostra com menos de 12 tags comuns, ou com tags numa única
      linha ou coluna, é recusada com a mensagem de R8.
- [x] **A numeração está correta.** Na visualização ao vivo, os IDs desenhados batem com a folha
      impressa, conferidos visualmente pelo usuário.
- [x] **O recálculo é reproduzível.** `--use_stored_values --target_yaml` sobre as imagens da
      execução AprilGrid reproduz o resultado da captura ao vivo.
- [ ] **O checker funciona com AprilGrid.** Com a calibração de fábrica, o checker mostra Stereo
      < 0,5 px nas poses testadas, na mesma faixa obtida com o xadrez (0,25–0,51 px).
- [x] **Erros de entrada são claros.** `target.yaml` inexistente, sem campo obrigatório ou com
      `target_type` diferente de `aprilgrid` encerra com mensagem clara e código de saída diferente
      de zero.
- [x] **O build não quebra.** As cinco ferramentas compilam no container. `--fisheye` e `--virtual`
      combinados com `--target_yaml` são aceitos pelo parse de argumentos.
- [x] **O README está atualizado** conforme R17.

## Perguntas em aberto

- **Resolvido:** o alvo está montado em placa rígida. O `target.yaml` do alvo é o nominal do Kalibr
  (`tagSize: 0.088`, `tagSpacing: 0.3`).
- NEEDS CLARIFICATION: os 88 mm e os 26,4 mm foram **medidos** na placa ou são os valores do
  arquivo? No xadrez, a impressão saiu 6,3 % menor que o nominal. Antes da validação, o tamanho da
  tag precisa ser conferido com régua ao longo de várias tags; se divergir, o `target.yaml` da
  validação deve trazer o valor medido. Isso bloqueia a validação, não o plano.

# Plano — Autocalibração no visualizador e `--svo` na calibração

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Etapas

- [x] 1. **Visualizador** (R1). `camera_disable_self_calib = true` logo antes do único
  `zed.open` (um só `InitParameters` serve ao vivo, SVO e estéreo virtual) e
  " * Self calibration: disabled" impresso. Verificado em 2026-11-04 com um SVO.
  - **Achado, anterior a esta mudança:** neste container o visualizador não abre
    (`CORRUPTED SDK INSTALLATION`), porque o `DEPTH_MODE::NEURAL` precisa do módulo de IA do SDK
    (cuDNN/TensorRT). Fora do escopo; fica registrado.

  Especificação original da etapa: Desligar a autocalibração nos `InitParameters` e registrá-la
  no início da execução.
  - **Pronto quando:** o binário compila; a execução ao vivo imprime "self calibration: disabled";
    e um `grep` confirma que nenhum caminho de abertura fica sem a opção.
- [x] 2. **`--svo` na calibração** (R2). `setFromSVOFile` com os mesmos parâmetros de abertura
  (autocalibração desligada) e " * Input: SVO file ..." no início. Recusa `--svo` com
  `--session`, `--timelapse` ou `--camera_settings` (descrevem uma câmera ao vivo) e com
  `--use_stored_values`, com código 1. No fim do arquivo (`END_OF_SVOFILE_REACHED`), calibra com o
  que foi coletado se houver o mínimo; senão sai com código 1 e mensagem.
  - Verificado em 2026-11-04 com um SVO de 45 quadros gravado da câmera de bancada (gravador
    descartável em `calib_runs/tools/svo_rec/`): a ferramenta abriu o SVO e parou no fim dele
    ("End of the SVO file with 0 samples"), o que uma câmera ao vivo nunca faz. Sem o alvo em
    casa, não houve amostras; calibrar a partir de um SVO com o alvo fica na bancada.

  Especificação original da etapa: Aplicar `setFromSVOFile`, avisar no início e recusar a
  combinação com `--session`.
  - **Pronto quando:** o critério do SVO de teste passa e a combinação com `--session` sai com
    código 1.
- [x] 3. **Regressão** (R3). Em 2026-11-04, `calib_runs/regress_chess/after_higiene.log`: o YAML
  é idêntico ao `baseline.yml` (`cmp`) e o log, ao `baseline.log`.

  Especificação original da etapa: Regressão (R3).
  - **Pronto quando:** a regressão do xadrez é idêntica ao `baseline.yml`.

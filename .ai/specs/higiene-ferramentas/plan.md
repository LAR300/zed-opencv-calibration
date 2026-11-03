# Plano — Autocalibração no visualizador e `--svo` na calibração

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Etapas

- [ ] 1. **Visualizador** (R1). Desligar a autocalibração nos `InitParameters` e registrá-la no
  início da execução.
  - **Pronto quando:** o binário compila; a execução ao vivo imprime "self calibration: disabled";
    e um `grep` confirma que nenhum caminho de abertura fica sem a opção.
- [ ] 2. **`--svo` na calibração** (R2). Aplicar `setFromSVOFile`, avisar no início e recusar a
  combinação com `--session`.
  - **Pronto quando:** o critério do SVO de teste passa e a combinação com `--session` sai com
    código 1.
- [ ] 3. **Regressão** (R3).
  - **Pronto quando:** a regressão do xadrez é idêntica ao `baseline.yml`.

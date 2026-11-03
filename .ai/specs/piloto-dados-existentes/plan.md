# Plano — Piloto das métricas nas capturas existentes

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- C++: `--detections_out` (persistencia-captura, etapa 9), o relatório (saida-calibracao) e
  `zed_export_factory`.
- Python: o núcleo, o M1 e o M2 com os testes verdes.

## Etapas

- [ ] 1. **Exportar as detecções e F** (R1). Para cada captura, rodar
  `--use_stored_values --detections_out` a partir de uma cópia de trabalho em `calib_runs/piloto/`.
  Exportar a F da S/N 35180697.
  - **Pronto quando:** os 4 `detections.json` existem e o leitor do núcleo os aceita; as pastas
    originais têm o mesmo `ls -l` e `md5sum` de antes.
- [ ] 2. **Calibrações** (R2). As 4 variantes por captura, com configs salvas.
  - **Pronto quando:** o modo compatível reproduz o YAML do C++ até 1e-6 relativo nas 3 capturas
    AprilGrid; as 16 calibrações têm YAML e relatório.
- [ ] 3. **M1** (R3). RMS, mapa de resíduos e BR por câmera para as 16 calibrações e para F.
  - **Pronto quando:** existem 17 relatórios M1 com as figuras.
- [ ] 4. **M2** (R3). A matriz de pares entre as calibrações AprilGrid (mesma variante), cada uma
  contra F, e as três referências "média".
  - **Pronto quando:** as matrizes e os mapas de calor existem para cada variante.
- [ ] 5. **Responder R4** e escrever `resultado.md` (R5).
  - **Pronto quando:** cada uma das seis perguntas tem um número e uma figura de apoio, e cada
    decisão pendente tem uma recomendação justificada.
- [ ] 6. **Revisão do usuário.**
  - **Pronto quando:** as decisões estão aprovadas e copiadas em `experimentos/spec.md`.

## Riscos

- **O BR do xadrez pode sair mal estimado** com poucos quadrados 2×2 exclusivos por imagem (9×6
  cantos dão 12 quadrados). Reportar o número de alvos virtuais e o intervalo por bootstrap.

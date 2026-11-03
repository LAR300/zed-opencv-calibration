# Spec — Autocalibração no visualizador e `--svo` na calibração

> **O quê, por quê e condições de implementação.** Origem: [`auditoria.md`](../../auditoria.md)
> §1.5 e §6 (riscos 9 e 10).

## Problema

- **`stereo_reprojection_viewer` abre a ZED 2i com a autocalibração do SDK ativa**
  (`stereo_reprojection_viewer/src/main.cpp:308-338`). O que se vê nele, inclusive com
  `--ocv` (arquivo OpenCV), vem de uma calibração que o SDK ajusta a cada execução. O brief exige
  a autocalibração desligada em todos os caminhos.
- **`zed_stereo_calibration --svo` é lido, mas não é aplicado** (`stereo_calibration/src/main.cpp:102`,
  `:123-124`). A ferramenta abre a câmera conectada, sem aviso, ou falha se não houver câmera.

## Requisitos

- **R1.** O visualizador abre a câmera com `camera_disable_self_calib = true` em todos os caminhos:
  ao vivo, SVO e câmera virtual.
- **R2.** `zed_stereo_calibration --svo <arquivo>` abre o SVO
  (`init_params.input.setFromSVOFile`) com a autocalibração desligada, como o `zed_stereo_checker`
  já faz (`stereo_checker/src/main.cpp:330-331`). A ferramenta informa no início que a entrada é
  um SVO.
- **R3.** A regressão do xadrez (`--use_stored_values`) não muda.

## Não-objetivos

- Mudar o que o visualizador mostra.
- Suportar SVO no modo sessão: os metadados de um SVO, como temperatura e uptime, não são os da
  captura original.

## Critérios de aceitação

- [ ] Num SVO de teste, gravado com o `ZED_Explorer` da ZED 2i de bancada,
      `zed_stereo_calibration --svo` mostra as imagens do SVO e não as da câmera conectada.
- [ ] O visualizador imprime ou registra a autocalibração desligada. O código tem a opção em todos
      os caminhos.
- [ ] A regressão do xadrez é idêntica.
- [ ] Com `--svo` e `--session` juntos, a ferramenta recusa com mensagem clara.

## Perguntas em aberto

Nenhuma.

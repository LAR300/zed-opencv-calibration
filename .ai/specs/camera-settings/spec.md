# Spec — Controles de captura fixos durante a calibração

> **O quê e por quê.** Fonte da verdade da intenção. **Sem decisão técnica nem passos de
> implementação** (esses vão em `decisions.md` e `plan.md`).

## Problema

`zed_stereo_calibration` e `zed_stereo_checker` não ajustam nenhum controle de captura. Exposição,
ganho e white balance ficam no que a câmera estiver usando, normalmente o automático. Isso traz três
problemas:

- **Imagens que variam durante a coleta.** O automático reajusta exposição e ganho conforme a cena
  e a posição do alvo. Com pouca luz, o tempo de exposição sobe e aparece borrão de movimento, que
  desvia cantos mesmo quando passa no filtro de nitidez. Ganho alto traz ruído.
- **Uso submerso (próxima etapa).** A luz muda com a profundidade e a turbidez, e o automático pode
  oscilar entre quadros. Também é preciso repetir as mesmas condições entre o ar e a água.
- **Nenhuma rastreabilidade.** Uma calibração não registra com que exposição, ganho, sharpness ou
  gamma as imagens foram feitas.

A nitidez digital (SHARPNESS) e o gamma mexem justamente nas bordas que a detecção usa para achar os
cantos, e hoje nem é possível saber que valores estavam valendo.

## Contexto

- Não há `setCameraSettings` em nenhuma ferramenta do repositório. O código só define resolução
  (`AUTO`, que na ZED 2i dá HD720) e 15 fps.
- Controles `sl::VIDEO_SETTINGS` do ZED SDK 5.1.0, lidos da ZED 2i (S/N 35180697, firmware 1523)
  logo depois de abrir, em 2026-11-03, com `calib_runs/tools/read_video_settings`. Os valores são o
  estado da câmera ao abrir, não necessariamente o padrão de fábrica:

  | Controle | Lido | Faixa | Notas do SDK |
  |---|---:|---|---|
  | `BRIGHTNESS` | 4 | 0–8 | não existe na ZED X |
  | `CONTRAST` | 4 | 0–8 | não existe na ZED X |
  | `HUE` | 0 | 0–11 | não existe na ZED X |
  | `SATURATION` | 4 | 0–8 | |
  | `SHARPNESS` | 4 | 0–8 | nitidez digital |
  | `GAMMA` | 5 | 1–9 | gamma do ISP |
  | `AEC_AGC` | 1 (auto) | 0–1 | definir `GAIN` ou `EXPOSURE` desliga o automático |
  | `GAIN` | 1 | 0–100 | automático quando `AEC_AGC` = 1 |
  | `EXPOSURE` | 50 | 0–100 | 100 ≈ 19,97 ms a 15 fps; 0 = 0,17 ms |
  | `AEC_AGC_ROI` | 0,0 1280×720 | retângulo | região usada pelo automático |
  | `WHITEBALANCE_AUTO` | 1 (auto) | 0–1 | |
  | `WHITEBALANCE_TEMPERATURE` | 4700 | 2800–6500, passo 100 | definir desliga o WB automático |
  | `LED_STATUS` | leitura falhou | 0–1 | depende do acesso ao HID da câmera |
  | `EXPOSURE_TIME`, `ANALOG_GAIN`, `DIGITAL_GAIN`, `AUTO_EXPOSURE_TIME_RANGE`, `AUTO_ANALOG_GAIN_RANGE`, `AUTO_DIGITAL_GAIN_RANGE`, `EXPOSURE_COMPENSATION`, `DENOISING`, `SCENE_ILLUMINANCE` | não suportados na ZED 2i | — | só ZED X / X One; `SCENE_ILLUMINANCE` é só leitura |

- O SDK ajusta alguns controles juntos: definir `GAIN` ou `EXPOSURE` põe `AEC_AGC` em 0, e definir
  `WHITEBALANCE_TEMPERATURE` põe `WHITEBALANCE_AUTO` em 0.
- O `LED_STATUS` falhou na leitura. Na ZED 2i ele passa pela interface HID (`2b03:f881`), que nesta
  bancada não é acessível ao usuário `admin` sem liberar o dispositivo.
- Com o acrílico na frente da câmera, o LED pode refletir na placa e aparecer na imagem.

## Requisitos

### Arquivo de controles
- **R1.** `zed_stereo_calibration` e `zed_stereo_checker` aceitam `--camera_settings <arquivo>`, um
  YAML com os controles a aplicar. Sem essa opção, as duas ferramentas não alteram nenhum controle,
  como hoje, mas **imprimem os valores em uso** ao iniciar (R7).
- **R2.** Os campos usam os nomes de `sl::VIDEO_SETTINGS`, como `EXPOSURE`, `GAIN`, `AEC_AGC`,
  `WHITEBALANCE_TEMPERATURE` e `SHARPNESS`. Só os controles presentes no arquivo são aplicados; os
  ausentes ficam como estão.
- **R3.** O arquivo consegue expressar tanto o modo automático (`AEC_AGC: 1`, `WHITEBALANCE_AUTO: 1`)
  quanto o manual (`EXPOSURE`, `GAIN`, `WHITEBALANCE_TEMPERATURE`). Combinações contraditórias, como
  `AEC_AGC: 1` junto com `EXPOSURE`, são um erro.

### Validação
- **R4.** As ferramentas recusam aplicar e encerram com erro claro e código de saída diferente de
  zero, **antes de iniciar a coleta ou a verificação**, nestes casos:
  - controle que a câmera conectada não suporta, como `EXPOSURE_TIME` na ZED 2i;
  - valor fora da faixa ou do passo (por exemplo `BRIGHTNESS: 9` ou `WHITEBALANCE_TEMPERATURE: 4750`);
  - nome desconhecido, tipo errado ou arquivo malformado;
  - controle só de leitura (`SCENE_ILLUMINANCE`).
- **R5.** Depois de aplicar, cada controle é **lido de volta**. Se algum valor não bate, é erro, pelo
  mesmo caminho de R4.

### Comportamento durante o uso
- **R6.** Os controles aplicados ficam **constantes durante toda a coleta ou verificação**.
- **R7.** Ao iniciar, as ferramentas imprimem o valor de cada controle suportado pela câmera, no
  estilo das informações de câmera que já imprimem, para o `calib.log` (`| tee`) registrar as
  condições da calibração:
  - **com `--camera_settings`:** os controles aplicados e o valor efetivo lido de volta;
  - **sem `--camera_settings`:** os valores em uso, sem alterar nenhum, indicando que vêm do estado
    atual da câmera (automático onde for o caso).
- **R8.** Ao sair, as ferramentas **restauram os valores que a câmera tinha antes**, nestes casos:
  - fim normal da calibração ou do checker;
  - saída com `q` ou ESC;
  - erro depois de abrir a câmera;
  - interrupção com Ctrl+C.

  Uma morte forçada (`kill -9`) não precisa ser coberta.

### Recálculo das imagens salvas
- **R12.** Com `--use_stored_values`, que não abre a câmera, `--camera_settings` é **ignorado com
  aviso**: a calibração roda normalmente e o terminal informa que a opção não tem efeito nesse modo.
  Também informa que os controles com que as imagens foram feitas estão no `calib.log` da captura,
  já que as imagens salvas não os guardam. É o mesmo tratamento que a spec `sample-limits` dá a
  `--min_samples` e `--max_samples`.

### Arquivo de exemplo e documentação
- **R9.** O repositório traz um arquivo de exemplo:
  - com os valores lidos em 2026-11-03 (tabela acima), com exposição e white balance em manual;
  - com um **comentário em cada controle informando a faixa aceita** e o que o valor faz;
  - com os controles da ZED X listados comentados, indicando que não existem na ZED 2i;
  - sem `LED_STATUS`, que não afeta a calibração (ver Não-objetivos).
- **R10.** O `README.md` documenta a opção, o formato do arquivo, a restauração ao sair e a
  recomendação de usar o mesmo arquivo em calibração e checker.
- **R11.** O código segue o estilo do repositório (C++17, parse de argumentos no padrão atual,
  mensagens em inglês, sem dependência nova além do OpenCV e do ZED SDK).

## Não-objetivos

- Definir os valores **ótimos** para calibrar: isso é experimento de bancada, feito depois com a
  ferramenta pronta.
- `AEC_AGC_ROI` (retângulo do automático) e controles diferentes por câmera (esquerda e direita).
- Ferramentas mono e o visualizador de reprojeção.
- Validar em ZED X / X One. Os controles delas são aceitos se a câmera os suportar, mas só a ZED 2i é
  testada.
- Mudar resolução ou fps.
- Corrigir o acesso ao HID na bancada (a regra udev ou o `chmod` usados para a IMU).
- `LED_STATUS`: não influencia a calibração de forma relevante e depende do acesso ao HID. Fica fora
  do arquivo de exemplo e não precisa ser validado.

## Critérios de aceitação

- [ ] **Sem `--camera_settings`,** nenhum controle muda (os valores lidos antes e depois de uma
      execução são iguais), e os valores em uso aparecem no início do `calib.log`.
- [ ] **Com o arquivo de exemplo,** todos os controles são aplicados, lidos de volta iguais e
      impressos no início; o `calib.log` mostra os valores.
- [ ] **Com exposição e ganho manuais,** os valores no fim da coleta são os mesmos do início.
- [ ] **Erros:** cada caso de R4 (`EXPOSURE_TIME`, `BRIGHTNESS: 9`, `WHITEBALANCE_TEMPERATURE: 4750`,
      `AEC_AGC: 1` com `EXPOSURE`, nome inexistente, YAML malformado) encerra com mensagem clara e
      código diferente de zero, sem iniciar a coleta.
- [ ] **Restauração:** depois de terminar a calibração, de sair com `q` e de interromper com Ctrl+C,
      `read_video_settings` mostra os mesmos valores de antes da execução.
- [ ] **O checker** aceita o mesmo arquivo e mostra os erros de reprojeção com os controles
      aplicados.
- [ ] **Sem regressão:** o recálculo `--use_stored_values` das imagens de xadrez de 16/10 continua
      idêntico ao `baseline.yml`, e o AprilGrid continua funcionando.
- [ ] **Com `--use_stored_values --camera_settings <arquivo>`,** a calibração roda normalmente e o
      terminal mostra o aviso de R12.
- [ ] **O arquivo de exemplo e o README** estão no repositório conforme R9 e R10.

## Perguntas em aberto

Nenhuma. As três perguntas foram fechadas em R7, R12 e nos Não-objetivos (`LED_STATUS`).

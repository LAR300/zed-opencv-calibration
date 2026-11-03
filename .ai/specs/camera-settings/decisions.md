# Decisões — Controles de captura fixos durante a calibração

> **Por que as escolhas técnicas foram feitas.** Estilo ADR leve. Durável: quando uma decisão
> mudar, registre a nova com o motivo.

### D1. Módulo compartilhado em duas camadas: leitura do arquivo (OpenCV) e aplicação (ZED SDK)

- **Contexto:** as duas ferramentas estéreo precisam ler o mesmo arquivo, validar, aplicar, ler de
  volta, imprimir e restaurar. A spec `aprilgrid-target` já criou `common/` para código
  compartilhado (D1 de lá) e testes sem câmera em `tests/`.
- **Decisão:** criar `common/camera_settings/` com duas partes:
  - **`camera_settings.hpp/.cpp`, só OpenCV:** lê o YAML com `cv::FileStorage`, confere os nomes,
    os tipos, os controles só de leitura ou fora de escopo, as combinações contraditórias e os
    passos fixos, e devolve uma lista ordenada de `nome → valor`. É testável em `tests/`;
  - **`zed_camera_settings.hpp/.cpp`, com o ZED SDK:** mapeia os nomes para `sl::VIDEO_SETTINGS`,
    confere suporte e faixa na câmera conectada, aplica, lê de volta, imprime, guarda o estado
    anterior e restaura.
- **Por quê:** a parte que erra fácil (nomes, conflitos, passos) fica coberta por testes sem
  câmera. A parte que depende do SDK fica pequena.
  - *Descartado:* um módulo único com o SDK, que impede os testes sem câmera, porque o executável de
    testes não liga o ZED SDK.
- **Consequências:** `tests/CMakeLists.txt` inclui `common/camera_settings/camera_settings.cpp`. Os
  dois `CMakeLists.txt` das ferramentas incluem as duas partes.

### D2. As faixas vêm da câmera; os passos e as regras, do código

- **Contexto:** a faixa válida de cada controle depende do modelo (BRIGHTNESS vai de 0 a 8 na ZED 2i
  e não existe na ZED X). O SDK informa a faixa com `getCameraSettingsRange` e o suporte com
  `isCameraSettingSupported`, mas não informa o passo de 100 K do `WHITEBALANCE_TEMPERATURE`.
- **Decisão:** a faixa e o suporte são conferidos **na câmera conectada**, depois de abrir e antes da
  coleta (R4). O passo de 100 K, os controles só de leitura (`SCENE_ILLUMINANCE`), os fora de escopo
  (`AEC_AGC_ROI`, `LED_STATUS`) e as combinações contraditórias ficam no código, na parte OpenCV.
- **Por quê:** não copia para o código uma tabela que o SDK já fornece e que muda entre modelos.
- **Consequências:** um arquivo inválido por nome ou conflito falha **antes** de abrir a câmera, e um
  inválido por faixa ou suporte falha **logo depois** de abrir. Os dois casos acontecem antes da
  coleta, como a spec pede.

### D3. Ordem de aplicação e de restauração respeita os acoplamentos do SDK

- **Contexto:** o SDK acopla controles: definir `GAIN` ou `EXPOSURE` põe `AEC_AGC` em 0, e definir
  `WHITEBALANCE_TEMPERATURE` põe `WHITEBALANCE_AUTO` em 0. Na ordem errada, a restauração deixaria
  a câmera em manual quando ela estava no automático.
- **Decisão:**
  - **Aplicar** os valores numéricos primeiro e as chaves de modo (`AEC_AGC`, `WHITEBALANCE_AUTO`)
    por último.
  - **Restaurar** na mesma ordem: valores e depois modos. Se a câmera estava no automático, o
    `AEC_AGC: 1` final devolve o automático.
  - Antes de aplicar, guardar **todos** os controles suportados, não só os do arquivo, porque um
    `EXPOSURE` no arquivo também muda o `AEC_AGC`.
- **Por quê:** garante R8 (voltar ao estado anterior) mesmo com os acoplamentos.
- **Consequências:** `AEC_AGC: 0` sozinho, sem `EXPOSURE` nem `GAIN`, é aceito: congela os valores que
  o automático escolheu naquele instante. A impressão de R7 mostra quais foram.
- **Verificado na etapa 4:** os valores precisam ser restaurados **todos**, inclusive os que um modo
  automático controla. Na ZED 2i, `WHITEBALANCE_TEMPERATURE` continua reportando o último valor
  definido mesmo com `WHITEBALANCE_AUTO` de volta em 1, por pelo menos 45 quadros. Uma variante que
  pulava esses valores deixou 4500 K no lugar de 4700 K e foi descartada. `GAIN` e `EXPOSURE`, ao
  contrário, acompanham o automático e mudam com a cena.

### D4. A leitura de volta espera alguns quadros

- **Contexto:** R5 exige ler de volta cada controle aplicado. Na ZED 2i (UVC), alguns controles podem
  só valer depois de alguns quadros, e uma leitura imediata poderia acusar um erro que não existe.
- **Decisão:** depois de aplicar, fazer alguns `grab()` (da ordem de 5 quadros, ~0,3 s a 15 fps) e só
  então ler de volta e comparar.
- **Por quê:** evita falsos erros sem atrasar o início de forma perceptível.
- **Consequências:** se a leitura de volta divergir mesmo assim, o erro mostra o pedido e o lido, e o
  número de quadros pode ser ajustado.

### D5. Ctrl+C restaura por meio de um sinal tratado

- **Contexto:** R8 inclui Ctrl+C. Hoje o SIGINT encerra o processo na hora, sem passar pelo
  `zed_camera.close()`. A calibração tem uma saída depois de abrir a câmera (erro ao criar a pasta
  das imagens) que não fecha a câmera, além do `q` e do fim normal. O checker sai por `break` no
  `q`.
- **Decisão:** um tratador de SIGINT só liga uma flag `std::atomic<bool>`. Os laços das duas
  ferramentas tratam essa flag como `q`. Todas as saídas depois de abrir a câmera passam por um
  único ponto que restaura e fecha, por meio de um objeto que restaura no destrutor e de chamadas
  explícitas antes de cada `return`.
- **Por quê:** restaurar dentro do tratador de sinal chamaria o SDK num contexto onde isso não é
  seguro. A flag mantém o trabalho no fluxo normal.
- **Consequências:** Ctrl+C durante o cálculo da calibração, que já roda depois de a câmera fechar,
  não precisa restaurar nada, porque os controles já foram restaurados ao fechar. `kill -9` fica
  fora, como a spec diz.

### D6. Arquivo de exemplo em `config/`

- **Decisão:** `config/camera_settings_zed2i.yaml`. Traz os valores lidos em 2026-11-03, com
  exposição e white balance em manual, e um comentário por controle com a faixa e o efeito na
  calibração. Os controles da ZED X ficam comentados e `LED_STATUS` fica de fora.
- **Por quê:** o repositório não tem pasta de configuração, e `config/` deixa claro que é ponto de
  partida, não código. O nome com o modelo evita confusão com uma futura ZED X.

### D7. Controles de faixa (`AUTO_*_RANGE`) ficam fora do arquivo

- **Contexto:** a spec (R2) diz que o arquivo usa os nomes de `sl::VIDEO_SETTINGS`, no formato
  `NOME: valor`. Três controles da ZED X (`AUTO_EXPOSURE_TIME_RANGE`, `AUTO_ANALOG_GAIN_RANGE` e
  `AUTO_DIGITAL_GAIN_RANGE`) recebem **dois** valores, mínimo e máximo, e não um. Isso apareceu na
  implementação da etapa 1.
- **Decisão:** a leitura recusa esses três com a mensagem "takes a min/max range and is not
  supported by this tool", como faz com `AEC_AGC_ROI`.
- **Por quê:** eles não existem na ZED 2i, a validação em ZED X é não-objetivo, e aceitá-los exigiria
  outro formato de valor só para eles.
- **Consequências:** se um dia uma ZED X for calibrada, esses controles precisam de uma extensão do
  formato.

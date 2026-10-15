# Objetivo, nomenclatura e decisões (v3)

Revisão v3: gabinete fechado, câmeras trocadas à mão num suporte único, 1 corpo de cubo com vários conjuntos de adesivos, upload por SWD.

## Objetivo do TCC

Escolher, entre as combinações Slave ML + câmera, a que será usada no produto final (1 slave + 1 câmera). Duas avaliações, depois comparadas:

- **(A) OFFLINE:** inferência sobre a database de imagens rotuladas, com medição de corrente por medidor externo (sem o Master).
- **(B) AO VIVO:** cada slave numa sessão com o Master (robô em malha fechada).

Conclusão = análise A x B e escolha da combinação (Etapa 17). Estratégia: ciclo completo com 1 câmera e 1 slave (MVP), depois escalar para 4 e fazer os afinamentos.

## Mecânica (nova)

- Gabinete único com todo o projeto dentro, tampando a luz externa: a **única fonte de luz é o WS2812** (os 3 presets são a variável de luz; sem luz ambiente a controlar).
- IHM (TFT, encoder, botões) fora do gabinete; alimentação por cabo de tomada macho (fonte AC-DC interna).
- Câmeras NÃO cabem juntas: **um suporte único, troca manual** (uma câmera por vez). Isso elimina o viés de posição/paralaxe do lado a lado (o quadrado latino de posições deixa de existir).
- **1 corpo de cubo** (peças coloridas). Os "outros cubos" são **conjuntos de adesivos** (sets) colados à mão sobre ele: variam tom e superfície, não o corpo.

## Nomenclatura

- **Master:** JL-32F4VE 2.0 (STM32F407VET6): estado do cubo, embaralhamento, log.
- **Slave atuador:** STM32F103C8T6: 6 motores A4988, WS2812, ESTOP. Fala com o Master por UART.
- **Slaves ML (4 pares, um por câmera):** ESP32-CAM + OV2640 · XIAO ESP32-S3 Sense + OV3660 (confirmar o sensor) · JL-32F4VE + OV7670 · Arduino Nano 33 BLE Sense Lite + OV7675. Webcam + script Python no PC = referência opcional, sem energia.
- **Medidor de corrente:** MCU externo + INA226 + SD (ou log serial no PC), fonte própria por slave.
- **Set:** conjunto de adesivos (`cube_id` no dataset passa a significar o set). Corpo do cubo = constante.

## Decisões

- Acurácia da visão = saída BRUTA do slave vs estado real. O Master NÃO repara.
- **Treino:** (1) base = treino único no PC, um modelo por câmera, int8, IGUAL em todos os MCUs (hash no log); (2) futuro (Etapa 19): treino/calibração embarcada, só depois da Etapa 17.
- Câmera vê 3 faces por foto; 2 poses fixas; troca de pose por sequência T de pares opostos que não move centros. T adotada: `D R2 L2 U D F B' R2 L'` (5 janelas; ver `poses.md`).
- Motores presos só aos centros: o centro NÃO entra na acurácia. Acurácia sobre as **48 facetas móveis** (24 por pose); reportar também por foto (24) e estado completo (48/48).
- Cada slave usa o **caminho de captura nativo** na coleta e no ao vivo (acaba a diferença "coletor x nativo"). Registradores fixos (exposição, ganho, balanço de branco), resolução e enquadramento padronizados por câmera.
- Imagens SEM compressão (RGB565/YUV) + sidecar. SD: Master guarda só o log; cada slave tem o seu; junção no PC.
- Divisão treino/teste por **TRIAL** (nunca por imagem/faceta): 70/30 (ou 60/40) + validação cruzada **por set** (deixa 1 set de fora) como análise secundária.
- Energia SEMPRE pelo medidor externo (fases marcadas, lotes de N, base ociosa). O Master NÃO lê INA226. A energia de A não inclui a leitura do SD.
- Tempos: UART com carimbo de fim de quadro + tempos internos do slave. EXTI/DWT e analisador lógico = `[AFINAR]`.
- **Upload por SWD (ST-Link V2 clone)**; CH340 só para log/serial e para o `flasher` das tabelas. DFU descartado.
- K1 no modo bancada = "pronto/pausa"; no modo APP continua "resolver".
- Critérios da conclusão fixados ANTES de olhar os resultados (17.1). Tudo aberto é `#define` em `BenchConfig.h`.

## Plano de coleta (database v1)

Mesmas seeds em todas as câmeras: o `Scrambler` é determinístico, então cada câmera vê a **mesma sequência de estados** (amostras pareadas por `seed` + índice do trial).

```
para cada SET de adesivos (laço externo: a troca de adesivo é a mais cara)
    para cada CÂMERA (ordem rotacionada entre os sets; troca manual, ROI conferida)
        para cada LUZ (ordem sorteada; troca é por comando, custo zero)
            N trials (embaralha -> A -> T -> B), checkpoints a cada CHECKPOINT_EVERY
```

- Rotacionar a ordem das câmeras entre os sets (quadrado latino 4x4/5x5 simples) evita confundir câmera com deriva do tempo (envelhecimento do LED, adesivo, temperatura).
- Registrar no sidecar: `set_id`, `mount_id` (nº da montagem/troca da câmera), `session_id`, temperatura interna se houver sensor.
- Após cada troca de câmera: verificar enquadramento com o overlay (10.6) e rodar bloco de controle (16.7).

## Marcos

| Marco | Etapa | Entrega                                                               |
| ----- | ----- | --------------------------------------------------------------------- |
| M1    | 9     | robô gira o cubo por comando, sem perder passo                        |
| M2    | 10    | primeiras fotos rotuladas (1 câmera, 2 poses), conferidas por overlay |
| M3    | 11    | primeiro modelo treinado no PC com acurácia medida (baseline v0)      |
| M4    | 12    | mesmo modelo rodando no ESP32-CAM sobre a database, acurácia ≈ PC     |
| M5    | 13    | ciclo fechado ao vivo: embaralha → foto → RESULT → compara com o real |
| M6    | 14    | database completa congelada (4 câmeras, N sets, 3 luzes)              |
| M7    | 15    | avaliação offline completa dos 4 slaves com energia                   |
| M8    | 16    | avaliação com o Master dos 4 slaves                                   |
| M9    | 17    | análise comparativa e escolha da combinação final                     |

## Estimativas (revisar no piloto 14.7)

Defaults provisórios (sets = 5, luzes = 3, trials por célula do dataset = 100, ao vivo = 50):

- Dataset: 1500 trials por câmera (3000 imagens); **6000 trials de robô no total**, porque agora as câmeras são coletadas em sequência (antes 1500, simultâneas).
- Ao vivo: 750 trials por slave; 3000 no total.
- Tempo por trial (chute, a medir): ~20 s (15 janelas de embaralho + T de 5 janelas + assentamento + 2 capturas) → ~8 h por câmera, ~33 h no total. Se o piloto confirmar, reduzir `DATASET_TRIALS_PER_CELL` e/ou `BENCH_SCRAMBLE_WINDOWS`.

Parâmetros efetivos e defaults: `lib/Robot/src/BenchConfig.h` (fonte da verdade). Pinos do Master: `stm32f4ve_peripherals.h`; do atuador: `actuator_pins.md`.

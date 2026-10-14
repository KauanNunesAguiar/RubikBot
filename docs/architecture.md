# Arquitetura do projeto

## 1. Peças

- **Master**: JL-32F4VE V2.0 (STM32F407VET6, 168 MHz). Guarda o estado do cubo, resolve, desenha, lê entradas e (na bancada) comanda o atuador.
- **Atuador**: STM32F103C8T6. Gira os 6 motores de passo (A4988). Ver [protocol.md](protocol.md) e [actuator_pins.md](actuator_pins.md).
- **Slaves ML** (futuro, Etapas 10+): câmeras + MCUs que classificam as cores. Ver [dataset.md](dataset.md) e [poses.md](poses.md).

## 2. Ambientes do PlatformIO

| Env        | Placa            | O que faz                                                         |
| ---------- | ---------------- | ----------------------------------------------------------------- |
| `master`   | `black_f407ve`   | firmware final (padrão); `-O2`, `SOLVER_EVENTS=1`                 |
| `flasher`  | `black_f407ve`   | recebe arquivos pela serial e grava na W25Q16                     |
| `sweep`    | `black_f407ve`   | varre `_p2Budget` com N cubos e imprime estatísticas (sem métricas e sem eventos) |
| `actuator` | `bluepill_f103c8`| firmware do atuador                                               |

## 3. Pastas

```
src/master/main.cpp        cria objetos, telas e tasks
src/flasher, src/sweep, src/actuator
lib/Board      TFT_FSMC, TouchXPT2046, W25Q16, Metrics, stm32f4ve_peripherals.h (pinos)
lib/CubeCore   CubeState, CubieCube, Coords, CubeSolver, PruneTable, SolverEvents
lib/CubeUI     CubeView (desenho + animação), CubeHUD (painel, estados, solve em segundo plano)
lib/Screens    Screen.h (interface + gerenciador), HudScreen, DebugScreen, TreeScreen
lib/Input      InputManager (encoder, K0, K1, touch -> fila de eventos)
lib/Robot      ActuatorLink, Scrambler, BenchConfig.h, poses.h
lib/Protocol   protocol.h (compartilhado Master <-> atuador)
tools/pc       gen_tables, pose_sim, proto_test (C++ do PC; build.ps1)
tools/py       flash_tables.py, sweep_log.py
tables/ build/ logs/   fora do Git
```

Dependências entre libs: `CubeCore -> Board` (`PruneTable.h` usa `W25Q16.h` e `Metrics.h`); `CubeUI -> CubeCore, Board`; `Screens -> CubeUI, Input`.

## 4. Tasks do FreeRTOS (Master)

| Task       | Função                         | Prio | Pilha (words) | Período |
| ---------- | ------------------------------ | ---- | ------------- | ------- |
| `input`    | `InputManager::task` (poll)    | 3    | 192           | 2 ms    |
| `dispatch` | `taskDispatch` (consome fila)  | 2    | 512           | bloqueia na fila |
| `display`  | `ScreenManager::task`          | 1    | 768           | 20 ms   |
| `solve`    | `CubeHUD::solveTask`           | 1    | 768           | 20 ms   |
| `metrics`  | dump serial pelo WKUP          | 1    | 512           | 50 ms   |
| `led`      | pisca D2                       | 1    | 128           | 250 ms  |

Regra: **só a task `display` desenha no TFT**, por isso não há mutex (`lock()/unlock()` do HUD são vazios). As prioridades 1 evitam tirar CPU do solver.

## 5. Fluxo de entrada

```
InputManager::poll (2 ms)      encoder, K0, K1 com debounce; touch a cada 20 ms
    -> fila de 16 eventos (InputEvent: tipo, a, b)
taskDispatch -> handleInput
    K0 longo       -> próxima tela (bloqueada durante busca/execução: canLeave())
    demais eventos -> tela atual (Screen::onInput)
HudScreen::onInput
```

| Evento         | Modo VISTA                   | Modo FACE                         |
| -------------- | ---------------------------- | --------------------------------- |
| giro do encoder| gira a câmera (±15°)         | gira a face selecionada           |
| clique encoder | embaralha                    | seleciona a próxima face          |
| encoder longo (0,7 s) | alterna VISTA/FACE    | alterna VISTA/FACE                |
| K0             | embaralha                    | embaralha                         |
| K1             | resolve                      | resolve                           |
| toque          | botões SCRAMBLE / SOLVE na tela | idem                           |

## 6. Estados do HUD

`ST_IDLE` -> (SOLVE) `ST_SEARCHING` -> `ST_SOLVING` -> `ST_SOLVED`, ou `ST_ERROR`.
Resolver manualmente: embaralhar arma o cronômetro; o primeiro giro manual muda para `ST_MANUAL`; ao resolver, `ST_SOLVED` e atualiza o recorde.
O painel só redesenha o que mudou (cache de texto de largura fixa) para não piscar. Entrada travada (`inputLocked`) durante busca, execução e embaralhamento.

## 7. Desenho do cubo

`CubeView` renderiza num `GFXcanvas16` de 200x200 (80.000 B) e envia o quadro inteiro com `setAddrWindow` + `pushColors`:

1. Para cada um dos 54 adesivos calcula centro, normal e eixos na geometria do cubo; durante um giro, os adesivos da camada (`dot(centro, eixo) > 0.5`) giram em torno do eixo da face antes da câmera.
2. Gira pela câmera (Y depois X), descarta faces de costas, projeta em perspectiva.
3. Ordena do mais longe ao mais perto (algoritmo do pintor) e preenche cada quadrado com 2 triângulos.
4. Se houver giro em andamento, 2 tampas escuras aparecem no plano interno para "abrir" o cubo.

Os giros esperam numa fila de 64; cada um anima por `msPerQuarter` (150 ms no `main`, o dobro para meia-volta) com suavização smoothstep. Medido: ~34 fps, quadro ~10 ms (push ~4,6 ms).

## 8. Hardware do Master

| Periférico   | Ligação                                                                              |
| ------------ | ------------------------------------------------------------------------------------ |
| TFT (ILI9341, 240x320) | FSMC Bank1/NE1, 16 bits; `A18` seleciona comando (`0x60000000`) ou dado (`0x60080000`); backlight PB1; ID lido = `0x009341` |
| Touch XPT2046 | SPI2 em PB13 (CLK), PB14 (MISO), PB15 (MOSI), CS PB12; PEN PC5 (não usado para detectar toque) |
| Flash W25Q16 | PB3 (CLK), PB4 (MISO), PB5 (MOSI), CS PB0                                           |
| Botões       | K0 PE4, K1 PE3, WKUP PA0 (liga em 3V3, dump das métricas)                           |
| Encoder      | CLK PA4, DT PA5, SW PA7 (**PA7 também é o LED D3**: não usar D3 com o encoder)      |
| LED D2       | PA6                                                                                  |
| Atuador      | USART3 (PB10/PB11), 115200                                                           |

Pinos completos e sinais não usados (SD, JTAG, nRF24, USB): `lib/Board/src/stm32f4ve_peripherals.h`.

## 9. Memória

- **Heap do FreeRTOS:** 119.488 B no total; o canvas ocupa 80.000 B (67%); sobram ~22,7 KB. Alvo: manter >= 8 KB livres de mínimo.
- **CCM RAM (64 KB, só CPU):** `_cc` (1320 B) e os 18 giros (720 B) do solver. Por isso só pode existir 1 `CubeSolver`. Nada de DMA/SD na CCM.
- **Flash externa:** tabelas de poda (ver [prune_tables.md](prune_tables.md)).

## 10. Boot (`setup()`)

`metricsInit` -> `CubeState::init` e `solver.begin` (gera os giros) -> `flash.begin` -> (opcional, `-D RUN_SELFTESTS=1`: poda, solver em 5 cubos, verify; levam ~15 s) -> TFT + canvas do cubo -> touch (segurar K0 ao ligar calibra) -> HUD -> telas (`hud`, `debug`, `tree`) -> cria as tasks -> `vTaskStartScheduler()`. O `loop()` nunca roda.

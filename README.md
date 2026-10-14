# Documentação do RubikBot

Cubo mágico resolvido por um Master STM32F407VE (Kociemba de duas fases, tabelas de poda na flash externa W25Q16), com TFT, entrada por encoder/botões/touch e, na bancada do TCC, um atuador STM32F103 que gira o cubo de verdade.

## Leia nesta ordem

| Arquivo                                  | O que explica                                                                          |
| ---------------------------------------- | -------------------------------------------------------------------------------------- |
| [architecture.md](architecture.md)       | Pastas, envs do PlatformIO, tasks do FreeRTOS, fluxo de entrada e de uma resolução, RAM |
| [cube_state.md](cube_state.md)           | Como o cubo é guardado na memória (54 adesivos e 8+12 peças), giros, conversões, verify |
| [solver.md](solver.md)                   | Como a busca da solução funciona (coordenadas, poda, fase 1, fase 2, estágios, refino)  |
| [prune_tables.md](prune_tables.md)       | Como as 4 tabelas de poda são geradas, empacotadas, gravadas e lidas                    |
| [protocol.md](protocol.md)               | Protocolo Master <-> atuador (COBS + CRC16) e o firmware do atuador                     |
| [actuator_pins.md](actuator_pins.md)     | Pinos do atuador (F103C8T6)                                                            |
| [poses.md](poses.md)                     | Poses A/B da câmera, sequência T e mapas de rótulos                                    |
| [dataset.md](dataset.md)                 | Formato do dataset, sidecar JSON e CSV de resultados                                   |

## Fontes da verdade (não duplicar em texto)

- Layout das tabelas na flash: `lib/CubeCore/src/PruneTable.h` (e `tools/py/flash_tables.py`, duplicado por enquanto).
- Geometria das faces: `CUBE_FACE_GEOM` em `lib/CubeCore/src/CubeState.cpp`.
- Poses e T: `lib/Robot/src/poses.h` (gerado por `tools/pc/pose_sim`).
- Parâmetros da bancada: `lib/Robot/src/BenchConfig.h`.
- Andamento do projeto: `checklist.txt`.

# Protocolo Master <-> Atuador

Fonte: `lib/Protocol/src/protocol.h` (sem dependência de STM32/Arduino; o Master, o atuador e os testes do PC incluem o mesmo arquivo). Serial 115200 8N1.

## 1. Quadro

Quadro cru: `[type, seq, len, payload (len bytes), crcHi, crcLo]`.

- `len <= 8` (`MAX_PAYLOAD`), então o quadro cru tem no máximo 13 bytes.
- CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`) sobre `type..payload`. Vetor de teste: `"123456789"` -> `0x29B1`.
- No fio: **COBS(quadro cru) + `0x00`**. COBS remove todos os zeros do quadro, então o `0x00` é um delimitador inequívoco e o receptor ressincroniza sozinho depois de qualquer lixo. Tamanho máximo no fio: 15 bytes (`MAX_FRAME`). Sem `malloc`.
- `proto::Parser::feed(byte, frame)` recebe byte a byte e devolve `true` quando fecha um quadro **válido**; quadros ruins (CRC, tamanho, COBS, estouro) são descartados e contados em `bad()`.

## 2. Mensagens

| Tipo           | Cód.   | Sentido | Payload                                          |
| -------------- | ------ | ------- | ------------------------------------------------ |
| `T_PING`       | `0x01` | M -> A  | nenhum                                           |
| `T_STATUS`     | `0x02` | A -> M  | `state, lastId, err`                             |
| `T_MOVE_WINDOW`| `0x10` | M -> A  | `id, faceA, turnsA, faceB (255 = nenhuma), turnsB` |
| `T_DONE`       | `0x11` | A -> M  | `id, result`                                     |
| `T_EN`         | `0x12` | M -> A  | `1` liga os motores, `0` solta                   |
| `T_LIGHT`      | `0x13` | M -> A  | preset (0 = apagado); ainda é stub               |
| `T_ESTOP`      | `0x14` | M -> A  | nenhum                                           |
| `T_MICROSTEP`  | `0x15` | M -> A  | `1, 2, 4, 8` ou `16`                             |

Estados: `ST_IDLE, ST_MOVING, ST_ESTOP, ST_DISABLED, ST_WDOG`.
Resultados do DONE: `R_OK, R_ESTOP, R_DISABLED, R_BUSY, R_BAD_ARGS`.

Uma **janela** de movimento é uma face, ou duas faces **opostas** girando juntas (`faceB == (faceA + 3) % 6`). Faces: 0..5 = U R F D L B; voltas 1 = horário, 2 = meia-volta, 3 = anti-horário.

## 3. Regras do atuador

- **`MOVE_WINDOW` é idempotente.** Cada janela tem um `id`. Se chegar o mesmo `id` do último concluído, o atuador **não gira de novo**: reenvia o DONE guardado. Se chegar o `id` em curso durante o giro, ignora. Isso permite ao Master reenviar quando o DONE se perde sem risco de girar o cubo duas vezes.
- Qualquer comando que não seja `MOVE_WINDOW` é respondido com `STATUS`.
- **Watchdog:** sem nenhum quadro válido por `PROTO_WATCHDOG_MS` (1000 ms) estando em `IDLE` ou `MOVING`, o atuador para os motores, solta o EN e vai para `ST_WDOG`. Por isso o Master manda `PING` a cada `BENCH_ACT_PING_MS` (300 ms) quando ocioso.
- **`T_EN` com 1** recomeça a sessão: zera a memória de ids (`haveLast = false`) e limpa o erro. Chame no início de cada sessão. Com o ESTOP pressionado, vai para `ST_ESTOP`.
- **ESTOP:** botão em PB8 (debounce ~5 ms) ou comando; para tudo, solta o EN e, se estava girando, responde o DONE pendente com `R_ESTOP`.
- `T_MICROSTEP` só é aceito parado e com valores 1/2/4/8/16 (A4988: MS1 MS2 MS3 = 000, 100, 010, 110, 111); a velocidade angular é mantida.

## 4. Lado do Master: `ActuatorLink`

Chamar sempre de **uma** task (bloqueia com `vTaskDelay`).

- `moveWindow(w)`: valida, gera o `id`, envia e **reenvia o mesmo quadro a cada `BENCH_ACT_RETRY_MS` (250 ms)** até chegar o DONE com esse id ou estourar `BENCH_ACT_MOVE_TIMEOUT_MS` (3000 ms).
- Retornos: `OK`, `ERR_TIMEOUT -1`, `ERR_ESTOP -2`, `ERR_DISABLED -3`, `ERR_BUSY -4`, `ERR_LINK -5`, `ERR_ARGS -6`.
- **Só aplicar o giro no `CubeState` quando `moveWindow` devolver `OK`.** Em erro ou timeout o estado real do cubo é desconhecido.
- `ping`, `enable`, `setLight`, `setMicrostep`, `estop`: comandos simples, concluídos pelo primeiro `STATUS`.

## 5. Firmware do atuador (`src/actuator/actuator.cpp`)

- Versão v0: velocidade constante, sem rampa.
- O TIM2 gera os pulsos numa única ISR: uma interrupção sobe os STEP dos eixos que ainda têm passos (`BSRR` em GPIOA), a seguinte desce e conta o passo. Isso permite 2 eixos opostos ao mesmo tempo sem pinos de timer dedicados.
- Passos por giro: `ACT_STEPS_PER_QUARTER` (200, com microstep 1/4) x 2 para meia-volta, escalado por `micro / 4`. Frequência `ACT_STEP_HZ` = 600 Hz: ~0,33 s por 90° e ~0,67 s por 180° (a janela dura o tempo do eixo mais longo).
- Sentido: horário em `turns` 1 e 2, anti-horário em 3; `ACT_DIR_INVERT` (bit por eixo) corrige o sentido real dos motores na calibração (checklist 9.6).
- EN nasce solto no boot (`HIGH` antes do `pinMode`).
- LED PC13: 500 ms parado, 100 ms em movimento/erro.

Pinos: [actuator_pins.md](actuator_pins.md).

## 6. Testes

- `tools/pc/proto_test` (`build/pc/proto_test.exe`): vetor do CRC, ida e volta de 3000 quadros, 1 bit trocado (rejeita e recupera), lixo antes do quadro, estouro de buffer e fuzz de 2e6 bytes.
- `tools/act_test.py COMx ping | en 0|1 | ms N | light N | estop | move FA TA [FB TB] [dup]`: fala com o atuador sem o Master. `dup` reenvia o mesmo id e confirma que não gira de novo.

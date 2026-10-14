# Pinos do atuador (STM32F103C8T6) — CONFIRMADO

Eixos: 0=U 1=R 2=F 3=D 4=L 5=B (mesma ordem do CubeFace). Janelas giram 2 eixos opostos juntos;
os pulsos saem de UMA ISR de timer que alterna qualquer GPIO, então STEP/DIR não precisam de pinos de timer.

| Função           | Pino(s)                                                                             | Observação                                                                |
| ---------------- | ----------------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| STEP U,R,F,D,L,B | PA0, PA1, PA4, PA5, PA6, PA7                                                        | pull-down externo em cada STEP                                            |
| DIR U,R,F,D,L,B  | PB0, PB1, PB10, PB11, PB12, PB13                                                    | sem função especial                                                       |
| EN (comum)       | PB14                                                                                | A4988 liga com EN BAIXO: pull-up externo (motores soltos no boot)         |
| MS1, MS2, MS3    | PB5, PB6, PB7                                                                       | comuns aos 6 drivers (microstep por comando)                              |
| UART do Master   | USART2: PA2 (TX), PA3 (RX)                                                          | cruzar com o Master: PB11 (RX) <- PA2, PB10 (TX) -> PA3; GND comum; 3,3 V |
| WS2812 DIN       | PA8                                                                                 | TIM1_CH1 + DMA; level shifter 74AHCT125 + 330 ohm                         |
| ESTOP            | PB8                                                                                 | botão para GND, pull-up; corta EN direto por hardware                     |
| LED da placa     | PC13                                                                                | heartbeat                                                                 |
| Reservados       | PA9/PA10 (USART1 upload/log), PA13/PA14 (SWD), PA11/PA12 (USB), PB3/PB4/PA15 (JTAG) | não usar                                                                  |

Protocolo: `lib/Robot/src/protocol.h`, 115200 8N1.

// BenchConfig.h — configuração da bancada (versão MÍNIMA do MVP; cresce por etapa).
// Tudo sobrescrevível por -D no platformio.ini. Os valores efetivos saem no log (benchPrintConfig).
#pragma once
#include <stdint.h>

/* --- Enlaces seriais (número do USART; pinos definidos na 9.5 / 10.1, conferir no esquemático) --- */
#ifndef LINK_ACT_UART
#define LINK_ACT_UART 3  // Master -> atuador (PB10/PB11)
#endif
#ifndef LINK_ACT_BAUD
#define LINK_ACT_BAUD 115200
#endif
#ifndef LINK_ML_UART
#define LINK_ML_UART 6  // Master -> slave ML (PC6/PC7), a conferir
#endif
#ifndef LINK_ML_BAUD
#define LINK_ML_BAUD 115200
#endif

/* --- Mecânica / ciclo --- */
#ifndef BENCH_SETTLE_MS
#define BENCH_SETTLE_MS 150  // assentamento após o giro (calibrar na 9.6)
#endif
#ifndef BENCH_SCRAMBLE_WINDOWS
#define BENCH_SCRAMBLE_WINDOWS 15  // janelas por embaralhamento
#endif
#ifndef CHECKPOINT_EVERY
#define CHECKPOINT_EVERY 20  // trials entre checkpoints de rótulo (14.5)
#endif
#ifndef BENCH_MAX_ATTEMPTS
#define BENCH_MAX_ATTEMPTS 3
#endif
#ifndef BENCH_ACK_TIMEOUT_MS
#define BENCH_ACK_TIMEOUT_MS 1000
#endif
#ifndef BENCH_RESULT_TIMEOUT_MS
#define BENCH_RESULT_TIMEOUT_MS 5000
#endif

/* --- Seeds (treino x teste nunca iguais; xorshift32 não aceita 0) --- */
#ifndef BENCH_SEED_TRAIN
#define BENCH_SEED_TRAIN 1001
#endif
#ifndef BENCH_SEED_TEST
#define BENCH_SEED_TEST 2002
#endif

/* --- TRIG para o slave ML: pino ainda PENDENTE. Ligar a feature sem definir o pino = erro --- */
#ifndef BENCH_USE_SLAVE_TRIG
#define BENCH_USE_SLAVE_TRIG 0
#endif
#if BENCH_USE_SLAVE_TRIG && !defined(PIN_SLAVE_TRIG)
#error "BENCH_USE_SLAVE_TRIG=1 exige PIN_SLAVE_TRIG (pino pendente: definir no esquemático)"
#endif

/* --- Validações em tempo de compilação --- */
#define BENCH_UART_OK(x) ((x) == 1 || (x) == 2 || (x) == 3 || (x) == 6)
static_assert(BENCH_UART_OK(LINK_ACT_UART), "LINK_ACT_UART deve ser 1, 2, 3 ou 6");
static_assert(BENCH_UART_OK(LINK_ML_UART), "LINK_ML_UART deve ser 1, 2, 3 ou 6");
static_assert(LINK_ACT_UART != LINK_ML_UART, "atuador e slave ML não podem usar o mesmo USART");
static_assert(LINK_ACT_BAUD > 0 && LINK_ML_BAUD > 0, "baud inválido");
static_assert(BENCH_SETTLE_MS <= 5000, "BENCH_SETTLE_MS absurdo");
static_assert(BENCH_SCRAMBLE_WINDOWS >= 1 && BENCH_SCRAMBLE_WINDOWS <= 60, "janelas: 1..60");
static_assert(CHECKPOINT_EVERY >= 1, "CHECKPOINT_EVERY >= 1");
static_assert(BENCH_MAX_ATTEMPTS >= 1 && BENCH_MAX_ATTEMPTS <= 10, "tentativas: 1..10");
static_assert(BENCH_ACK_TIMEOUT_MS > 0 && BENCH_RESULT_TIMEOUT_MS > 0, "timeouts > 0");
static_assert(BENCH_SEED_TRAIN != 0 && BENCH_SEED_TEST != 0, "seed 0 trava o xorshift");
static_assert(BENCH_SEED_TRAIN != BENCH_SEED_TEST, "seed de treino == seed de teste");

#ifdef ARDUINO
#include <Arduino.h>
/* Imprime os valores efetivos (chamar no boot e copiar para o cabeçalho do CSV). */
static inline void benchPrintConfig(Print& out) {
	out.printf("[CFG] act=USART%d@%lu ml=USART%d@%lu settle_ms=%u windows=%u checkpoint=%u attempts=%u\n",
	           (int)LINK_ACT_UART, (unsigned long)LINK_ACT_BAUD, (int)LINK_ML_UART, (unsigned long)LINK_ML_BAUD,
	           (unsigned)BENCH_SETTLE_MS, (unsigned)BENCH_SCRAMBLE_WINDOWS, (unsigned)CHECKPOINT_EVERY,
	           (unsigned)BENCH_MAX_ATTEMPTS);
	out.printf("[CFG] ack_to=%u result_to=%u seed_train=%lu seed_test=%lu\n", (unsigned)BENCH_ACK_TIMEOUT_MS,
	           (unsigned)BENCH_RESULT_TIMEOUT_MS, (unsigned long)BENCH_SEED_TRAIN, (unsigned long)BENCH_SEED_TEST);
}
#endif
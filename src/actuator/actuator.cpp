// actuator.cpp — firmware v0 do atuador (STM32F103C8T6). Velocidade constante, sem rampa.
// Pinos: docs/actuator_pins.md. Protocolo: lib/Robot/src/protocol.h
#include <Arduino.h>

#include "protocol.h"
using namespace proto;

#ifndef ACT_STEPS_PER_QUARTER
#define ACT_STEPS_PER_QUARTER 200  // passos (microstep) por 90 graus, com microstep 1/4
#endif
#ifndef ACT_STEP_HZ
#define ACT_STEP_HZ 600  // frequência de passo com microstep 1/4 (conservadora; sem rampa)
#endif
#ifndef ACT_DIR_INVERT
#define ACT_DIR_INVERT 0  // bit i = 1 inverte o sentido do eixo i (acertar na calibração 9.6)
#endif
#ifndef ACT_BAUD
#define ACT_BAUD 115200
#endif

static const uint32_t STEP_PIN[6] = {PA0, PA1, PA4, PA5, PA6, PA7};  // todos em GPIOA
static const uint8_t STEP_BIT[6] = {0, 1, 4, 5, 6, 7};
static const uint32_t STEP_MASK = (1u << 0) | (1u << 1) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7);
static const uint32_t DIR_PIN[6] = {PB0, PB1, PB10, PB11, PB12, PB13};
static const uint32_t PIN_EN = PB14, PIN_MS1 = PB5, PIN_MS2 = PB6, PIN_MS3 = PB7, PIN_ESTOP = PB8, PIN_LED = PC13;

#define link Serial2
static HardwareTimer* tim;

/* ---------------- movimento (ISR) ---------------- */
static volatile uint32_t rem[6];
static volatile bool hi = false, moveDone = false;

static void isr() {
	if (!hi) {  // subida: levanta o STEP dos eixos que ainda têm passos
		uint32_t m = 0;
		for (uint8_t i = 0; i < 6; i++)
			if (rem[i]) m |= 1u << STEP_BIT[i];
		if (m) {
			GPIOA->BSRR = m;
			hi = true;
		} else {
			tim->pause();
			moveDone = true;
		}
	} else {  // descida: baixa tudo e conta o passo
		GPIOA->BSRR = STEP_MASK << 16;
		hi = false;
		uint32_t any = 0;
		for (uint8_t i = 0; i < 6; i++) {
			if (rem[i]) rem[i]--;
			any |= rem[i];
		}
		if (!any) {
			tim->pause();
			moveDone = true;
		}
	}
}

/* ---------------- estado ---------------- */
static uint8_t state = ST_DISABLED, err = 0, curId = 0, lastId = 0, lastRes = 0, light = 0, micro = 4, txSeq = 0;
static uint16_t stepHz = ACT_STEP_HZ;  // base no microstep 1/4; mudada por T_SPEED
static bool haveLast = false;
static uint32_t lastRx = 0;
static Parser parser;

static void send(uint8_t type, const uint8_t* pl, uint8_t len) {
	Frame f;
	f.type = type;
	f.seq = txSeq++;
	f.len = len;
	for (uint8_t i = 0; i < len; i++) f.payload[i] = pl[i];
	uint8_t buf[MAX_FRAME];
	size_t n = encode(f, buf);
	if (n) link.write(buf, n);
}
static void sendStatus() {
	uint8_t p[3] = {state, lastId, err};
	send(T_STATUS, p, 3);
}
static void sendDone(uint8_t id, uint8_t res) {
	uint8_t p[2] = {id, res};
	send(T_DONE, p, 2);
}

static void haltMotion() {
	tim->pause();
	for (uint8_t i = 0; i < 6; i++) rem[i] = 0;
	GPIOA->BSRR = STEP_MASK << 16;
	hi = false;
	moveDone = false;
}

// para tudo, solta o EN; se estava girando, responde o DONE pendente com 'result'
static void stopAll(uint8_t newState, uint8_t result, uint8_t errCode) {
	haltMotion();
	digitalWrite(PIN_EN, HIGH);
	if (state == ST_MOVING) {
		lastId = curId;
		lastRes = result;
		haveLast = true;
		sendDone(curId, result);
	}
	state = newState;
	if (errCode) err = errCode;
}

static void applyMicrostep(uint8_t div) {  // A4988: 1=000 2=100 4=010 8=110 16=111 (MS1 MS2 MS3)
	bool a = false, b = false, c = false;
	switch (div) {
		case 2:
			a = true;
			break;
		case 4:
			b = true;
			break;
		case 8:
			a = b = true;
			break;
		case 16:
			a = b = c = true;
			break;
		default:
			break;  // 1
	}
	digitalWrite(PIN_MS1, a);
	digitalWrite(PIN_MS2, b);
	digitalWrite(PIN_MS3, c);
	micro = div;
	tim->setOverflow(2u * stepHz * div / 4u, HERTZ_FORMAT);
}

static bool estopPressed() { return digitalRead(PIN_ESTOP) == LOW; }

static uint32_t stepsFor(uint8_t turns) { return (uint32_t)ACT_STEPS_PER_QUARTER * (turns == 2 ? 2 : 1) * micro / 4; }
static void setDir(uint8_t axis, uint8_t turns) {
	bool cw = (turns != 3);
	digitalWrite(DIR_PIN[axis], (cw ? 1 : 0) ^ ((ACT_DIR_INVERT >> axis) & 1));
}

static void onMove(const Frame& f) {
	if (f.len < 5) {
		sendDone(0, R_BAD_ARGS);
		return;
	}
	const uint8_t id = f.payload[0], fa = f.payload[1], ta = f.payload[2], fb = f.payload[3], tb = f.payload[4];

	if (state == ST_MOVING) {  // reenvio do mesmo id durante o giro: ignora
		if (id != curId) sendDone(id, R_BUSY);
		return;
	}
	if (haveLast && id == lastId) {  // DONE perdido: reenvia, NÃO gira de novo
		sendDone(lastId, lastRes);
		return;
	}
	bool okArgs = fa < 6 && ta >= 1 && ta <= 3;
	if (okArgs) {
		if (fb == 255)
			okArgs = (tb == 0);
		else
			okArgs = (fb == (fa + 3) % 6) && tb >= 1 && tb <= 3;
	}
	if (!okArgs) {
		sendDone(id, R_BAD_ARGS);
		return;
	}
	if (state == ST_ESTOP) {
		sendDone(id, R_ESTOP);
		return;
	}
	if (state != ST_IDLE) {
		sendDone(id, R_DISABLED);
		return;
	}

	setDir(fa, ta);
	if (fb != 255) setDir(fb, tb);
	delayMicroseconds(5);  // setup de DIR do A4988
	curId = id;
	state = ST_MOVING;
	haltMotion();
	rem[fa] = stepsFor(ta);
	if (fb != 255) rem[fb] = stepsFor(tb);
	tim->setCount(0);
	tim->resume();
}

static void onJog(const Frame& f) {
	if (f.len < 4) {
		sendDone(0, R_BAD_ARGS);
		return;
	}
	const uint8_t id = f.payload[0], axis = f.payload[1];
	const int16_t steps = (int16_t)(f.payload[2] | (f.payload[3] << 8));

	if (state == ST_MOVING) {
		if (id != curId) sendDone(id, R_BUSY);
		return;
	}
	if (haveLast && id == lastId) {
		sendDone(lastId, lastRes);
		return;
	}
	if (axis >= 6 || steps == 0) {
		sendDone(id, R_BAD_ARGS);
		return;
	}
	if (state == ST_ESTOP) {
		sendDone(id, R_ESTOP);
		return;
	}
	if (state != ST_IDLE) {
		sendDone(id, R_DISABLED);
		return;
	}

	digitalWrite(DIR_PIN[axis], (steps > 0 ? 1 : 0) ^ ((ACT_DIR_INVERT >> axis) & 1));
	delayMicroseconds(5);
	curId = id;
	state = ST_MOVING;
	haltMotion();
	rem[axis] = (uint32_t)(steps > 0 ? steps : -steps);
	tim->setCount(0);
	tim->resume();
}

static void handle(const Frame& f) {
	switch (f.type) {
		case T_MOVE_WINDOW:
			onMove(f);
			return;

		case T_EN:
			if (f.len >= 1 && f.payload[0]) {
				if (estopPressed()) {
					stopAll(ST_ESTOP, R_ESTOP, 1);
				} else if (state != ST_MOVING) {
					digitalWrite(PIN_EN, LOW);
					state = ST_IDLE;
					err = 0;
					haveLast = false;  // sessão nova: o Master recomeça os ids
				}
			} else {
				stopAll(ST_DISABLED, R_DISABLED, 0);
			}
			break;

		case T_ESTOP:
			stopAll(ST_ESTOP, R_ESTOP, 1);
			break;

		case T_LIGHT:
			if (f.len >= 1) light = f.payload[0];  // 9.7 liga isto aos WS2812
			break;

		case T_MICROSTEP:
			if (state != ST_MOVING && f.len >= 1 &&
			    (f.payload[0] == 1 || f.payload[0] == 2 || f.payload[0] == 4 || f.payload[0] == 8 ||
			     f.payload[0] == 16))
				applyMicrostep(f.payload[0]);
			else
				err = 3;
			break;

		case T_JOG:
			onJog(f);
			return;

		case T_SPEED:
			if (state != ST_MOVING && f.len >= 2) {
				uint16_t hz = f.payload[0] | (f.payload[1] << 8);
				if (hz >= 50 && hz <= 4000) {
					stepHz = hz;
					applyMicrostep(micro);
				} else {
					err = 3;
				}
			} else {
				err = 3;
			}
			break;

		default:
			break;  // T_PING e desconhecidos: só responde STATUS
	}
	sendStatus();
}

void setup() {
	digitalWrite(PIN_EN, HIGH);  // antes do pinMode: motores soltos desde o primeiro instante
	pinMode(PIN_EN, OUTPUT);
	for (uint8_t i = 0; i < 6; i++) {
		digitalWrite(STEP_PIN[i], LOW);
		pinMode(STEP_PIN[i], OUTPUT);
		pinMode(DIR_PIN[i], OUTPUT);
	}
	pinMode(PIN_MS1, OUTPUT);
	pinMode(PIN_MS2, OUTPUT);
	pinMode(PIN_MS3, OUTPUT);
	pinMode(PIN_ESTOP, INPUT_PULLUP);
	pinMode(PIN_LED, OUTPUT);

	tim = new HardwareTimer(TIM2);
	tim->attachInterrupt(isr);
	tim->pause();
	applyMicrostep(4);
	link.begin(ACT_BAUD);
	lastRx = millis();
}

void loop() {
	const uint32_t now = millis();

	Frame f;
	while (link.available())
		if (parser.feed((uint8_t)link.read(), f)) {
			lastRx = now;
			handle(f);
		}

	if (moveDone && state == ST_MOVING) {
		moveDone = false;
		lastId = curId;
		lastRes = R_OK;
		haveLast = true;
		state = ST_IDLE;
		sendDone(curId, R_OK);
	}

	static uint8_t estopCnt = 0;  // ~5 ms de botão apertado
	static uint32_t lastPoll = 0;
	if (now != lastPoll) {
		lastPoll = now;
		estopCnt = estopPressed() ? (estopCnt < 255 ? estopCnt + 1 : 255) : 0;
		if (estopCnt >= 5 && state != ST_ESTOP) stopAll(ST_ESTOP, R_ESTOP, 1);
	}

	if ((state == ST_IDLE || state == ST_MOVING) && now - lastRx > PROTO_WATCHDOG_MS) stopAll(ST_WDOG, R_DISABLED, 2);

	static uint32_t lastLed = 0;
	if (now - lastLed >= (state == ST_IDLE ? 500u : 100u)) {
		lastLed = now;
		digitalToggle(PIN_LED);
	}
}
// ActuatorLink.cpp
#include "ActuatorLink.h"

#include <STM32FreeRTOS.h>

void ActuatorLink::send(uint8_t type, const uint8_t* pl, uint8_t len) {
	proto::Frame f;
	f.type = type;
	f.seq = _seq++;
	f.len = len;
	for (uint8_t i = 0; i < len; i++) f.payload[i] = pl[i];
	uint8_t buf[proto::MAX_FRAME];
	size_t n = proto::encode(f, buf);
	if (n) _s.write(buf, n);
}

// lê o que chegou; atualiza o STATUS; true se chegou um quadro do tipo 'want'
bool ActuatorLink::pump(uint8_t want, proto::Frame& f) {
	while (_s.available()) {
		if (!_p.feed((uint8_t)_s.read(), f)) continue;
		if (f.type == proto::T_STATUS && f.len >= 3) {
			_state = f.payload[0];
			_lastId = f.payload[1];
			_err = f.payload[2];
		}
		if (f.type == want) return true;
	}
	return false;
}

// comando simples: envia (reenvia a cada BENCH_ACT_RETRY_MS) até chegar um STATUS
bool ActuatorLink::cmd(uint8_t type, const uint8_t* pl, uint8_t len, uint32_t timeoutMs) {
	while (_s.available()) _s.read();  // descarta resto antigo
	proto::Frame f;
	const uint32_t t0 = millis();
	uint32_t lastTx = 0;
	bool sent = false;
	for (;;) {
		uint32_t now = millis();
		if (now - t0 > timeoutMs) return false;
		if (!sent || now - lastTx >= BENCH_ACT_RETRY_MS) {
			send(type, pl, len);
			sent = true;
			lastTx = now;
		}
		if (pump(proto::T_STATUS, f)) return true;
		vTaskDelay(1);
	}
}

bool ActuatorLink::ping(uint32_t t) { return cmd(proto::T_PING, nullptr, 0, t); }
bool ActuatorLink::enable(bool on, uint32_t t) {
	uint8_t p = on ? 1 : 0;
	return cmd(proto::T_EN, &p, 1, t);
}
bool ActuatorLink::setLight(uint8_t preset, uint32_t t) { return cmd(proto::T_LIGHT, &preset, 1, t); }
bool ActuatorLink::setMicrostep(uint8_t div, uint32_t t) { return cmd(proto::T_MICROSTEP, &div, 1, t); }
bool ActuatorLink::estop(uint32_t t) { return cmd(proto::T_ESTOP, nullptr, 0, t); }

int ActuatorLink::moveWindow(const ScrWindow& w, uint32_t timeoutMs) {
	if (w.faceA > 5 || w.turnsA < 1 || w.turnsA > 3) return ERR_ARGS;
	uint8_t fb = 255, tb = 0;
	if (w.turnsB) {
		if (w.faceB != (w.faceA + 3) % 6 || w.turnsB > 3) return ERR_ARGS;  // só pares opostos
		fb = w.faceB;
		tb = w.turnsB;
	}
	const uint8_t id = ++_winId;
	const uint8_t pl[5] = {id, w.faceA, w.turnsA, fb, tb};

	while (_s.available()) _s.read();
	proto::Frame f;
	const uint32_t t0 = millis();
	uint32_t lastTx = 0;
	bool sent = false;
	for (;;) {
		uint32_t now = millis();
		if (now - t0 > timeoutMs) return ERR_TIMEOUT;
		if (!sent || now - lastTx >= BENCH_ACT_RETRY_MS) {  // mesmo id: o atuador não gira duas vezes
			send(proto::T_MOVE_WINDOW, pl, 5);
			sent = true;
			lastTx = now;
		}
		if (pump(proto::T_DONE, f) && f.len >= 2 && f.payload[0] == id) {
			switch (f.payload[1]) {
				case proto::R_OK:
					return OK;
				case proto::R_ESTOP:
					return ERR_ESTOP;
				case proto::R_DISABLED:
					return ERR_DISABLED;
				case proto::R_BUSY:
					return ERR_BUSY;
				case proto::R_BAD_ARGS:
					return ERR_ARGS;
				default:
					return ERR_LINK;
			}
		}
		vTaskDelay(1);
	}
}
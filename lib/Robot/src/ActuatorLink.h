// ActuatorLink.h — lado Master do enlace com o atuador. Chamar de UMA task (ex.: "ctrl"); bloqueia com vTaskDelay.
// Uso:   HardwareSerial actSerial(PB11, PB10);   // USART3 (RX, TX)
//        ActuatorLink act(actSerial);   act.begin();
//        int r = act.moveWindow(w);     // OK => aplicar no CubeState (cube.move(...)) SÓ AGORA
// Se moveWindow der erro/timeout, o estado real do cubo é desconhecido: NÃO aplicar no CubeState.
#pragma once
#include <Arduino.h>

#include "BenchConfig.h"
#include "Scrambler.h"
#include "protocol.h"

class ActuatorLink {
   public:
	enum Result : int8_t {
		OK = 0,
		ERR_TIMEOUT = -1,
		ERR_ESTOP = -2,
		ERR_DISABLED = -3,
		ERR_BUSY = -4,
		ERR_LINK = -5,
		ERR_ARGS = -6
	};

	explicit ActuatorLink(HardwareSerial& s) : _s(s) {}
	void begin(uint32_t baud = LINK_ACT_BAUD) { _s.begin(baud); }

	bool ping(uint32_t timeoutMs = 200);  // true se o atuador respondeu (também serve de keep-alive)
	bool enable(bool on, uint32_t timeoutMs = 500);
	bool setLight(uint8_t preset, uint32_t timeoutMs = 500);
	bool setMicrostep(uint8_t div, uint32_t timeoutMs = 500);  // 1, 2, 4, 8, 16
	bool estop(uint32_t timeoutMs = 300);
	int moveWindow(const ScrWindow& w, uint32_t timeoutMs = BENCH_ACT_MOVE_TIMEOUT_MS);

	uint8_t state() const { return _state; }  // último STATUS recebido (proto::State; 255 = nenhum)
	uint8_t lastErr() const { return _err; }
	uint32_t badFrames() const { return _p.bad(); }

   private:
	HardwareSerial& _s;
	proto::Parser _p;
	uint8_t _seq = 0, _winId = 0, _state = 255, _lastId = 0, _err = 0;

	void send(uint8_t type, const uint8_t* pl, uint8_t len);
	bool pump(uint8_t want, proto::Frame& f);
	bool cmd(uint8_t type, const uint8_t* pl, uint8_t len, uint32_t timeoutMs);
};
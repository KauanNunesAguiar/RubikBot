// protocol.h — protocolo Master <-> Atuador. Sem dependência de STM32/Arduino (o atuador inclui o mesmo arquivo).
// Quadro cru: [type, seq, len, payload(len), crcHi, crcLo]  (CRC16-CCITT-FALSE sobre type..payload)
// No fio: COBS(quadro cru) + 0x00. Sem malloc.
//
// Regras do atuador:
//  - MOVE_WINDOW traz um id. Se id == último id: NÃO gira; reenvia o DONE guardado (ou ignora se ainda girando).
//  - Qualquer comando que não seja MOVE_WINDOW é respondido com STATUS.
//  - Sem nenhum quadro válido por PROTO_WATCHDOG_MS: para os motores e solta EN (estado ST_WDOG).
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef PROTO_WATCHDOG_MS
#define PROTO_WATCHDOG_MS 1000
#endif

namespace proto {

enum Type : uint8_t {
	T_PING = 0x01,         // M->A, sem payload
	T_STATUS = 0x02,       // A->M: state, lastId, err
	T_MOVE_WINDOW = 0x10,  // M->A: id, faceA, turnsA, faceB (255 = nenhuma), turnsB
	T_DONE = 0x11,         // A->M: id, result
	T_EN = 0x12,           // M->A: 1 liga motores, 0 solta
	T_LIGHT = 0x13,        // M->A: preset (0 = apagado)
	T_ESTOP = 0x14,        // M->A: para e solta EN
	T_MICROSTEP = 0x15     // M->A: 1, 2, 4, 8 ou 16
};
enum State : uint8_t { ST_IDLE, ST_MOVING, ST_ESTOP, ST_DISABLED, ST_WDOG };
enum Result : uint8_t { R_OK, R_ESTOP, R_DISABLED, R_BUSY, R_BAD_ARGS };

constexpr size_t MAX_PAYLOAD = 8;
constexpr size_t RAW_MAX = 3 + MAX_PAYLOAD + 2;
constexpr size_t MAX_FRAME = RAW_MAX + 2;  // COBS (+1) e delimitador (+1)

struct Frame {
	uint8_t type, seq, len;
	uint8_t payload[MAX_PAYLOAD];
};

inline uint16_t crc16(const uint8_t* p, size_t n) {  // CRC-16/CCITT-FALSE: "123456789" -> 0x29B1
	uint16_t c = 0xFFFF;
	while (n--) {
		c ^= (uint16_t)(*p++) << 8;
		for (int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
	}
	return c;
}

inline size_t cobsEncode(const uint8_t* in, size_t n, uint8_t* out) {
	size_t ci = 0, w = 1;
	uint8_t code = 1;
	for (size_t i = 0; i < n; i++) {
		if (in[i] == 0) {
			out[ci] = code;
			ci = w++;
			code = 1;
		} else {
			out[w++] = in[i];
			if (++code == 0xFF) {
				out[ci] = code;
				ci = w++;
				code = 1;
			}
		}
	}
	out[ci] = code;
	return w;
}

// retorna o tamanho decodificado, ou 0 se inválido
inline size_t cobsDecode(const uint8_t* in, size_t n, uint8_t* out, size_t cap) {
	size_t r = 0, w = 0;
	while (r < n) {
		uint8_t code = in[r++];
		if (code == 0) return 0;
		for (uint8_t i = 1; i < code; i++) {
			if (r >= n || w >= cap || in[r] == 0) return 0;
			out[w++] = in[r++];
		}
		if (code != 0xFF && r < n) {
			if (w >= cap) return 0;
			out[w++] = 0;
		}
	}
	return w;
}

// escreve o quadro no fio (COBS + 0x00). out precisa de MAX_FRAME bytes. Retorna o tamanho, 0 se len inválido.
inline size_t encode(const Frame& f, uint8_t* out) {
	if (f.len > MAX_PAYLOAD) return 0;
	uint8_t raw[RAW_MAX];
	size_t n = 0;
	raw[n++] = f.type;
	raw[n++] = f.seq;
	raw[n++] = f.len;
	for (uint8_t i = 0; i < f.len; i++) raw[n++] = f.payload[i];
	uint16_t c = crc16(raw, n);
	raw[n++] = (uint8_t)(c >> 8);
	raw[n++] = (uint8_t)c;
	size_t e = cobsEncode(raw, n, out);
	out[e++] = 0;
	return e;
}

inline bool decode(const uint8_t* enc, size_t n, Frame& f) {
	uint8_t raw[RAW_MAX];
	size_t r = cobsDecode(enc, n, raw, sizeof(raw));
	if (r < 5) return false;
	uint8_t len = raw[2];
	if (len > MAX_PAYLOAD || r != (size_t)(3 + len + 2)) return false;
	uint16_t c = (uint16_t)((raw[r - 2] << 8) | raw[r - 1]);
	if (crc16(raw, r - 2) != c) return false;
	f.type = raw[0];
	f.seq = raw[1];
	f.len = len;
	for (uint8_t i = 0; i < len; i++) f.payload[i] = raw[3 + i];
	return true;
}

// Receptor byte a byte. feed() retorna true quando um quadro VÁLIDO acabou de fechar.
class Parser {
   public:
	bool feed(uint8_t b, Frame& out) {
		if (b != 0) {
			if (_n < sizeof(_buf))
				_buf[_n++] = b;
			else
				_over = true;
			return false;
		}
		if (_n == 0 && !_over) return false;  // delimitador solto
		bool ok = !_over && decode(_buf, _n, out);
		if (!ok) _bad++;
		_n = 0;
		_over = false;
		return ok;
	}
	uint32_t bad() const { return _bad; }  // quadros descartados (CRC, tamanho, COBS)

   private:
	uint8_t _buf[MAX_FRAME];
	size_t _n = 0;
	bool _over = false;
	uint32_t _bad = 0;
};

}  // namespace proto
// proto_test.cpp — testa protocol.h no PC
// g++ -O2 -std=c++17 -Ilib/Robot/src tools/proto_test/proto_test.cpp -o proto_test
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "protocol.h"
using namespace proto;

static uint32_t g = 12345;
static uint32_t rnd() {
	g ^= g << 13;
	g ^= g >> 17;
	g ^= g << 5;
	return g;
}
static int fails = 0;
static void chk(bool ok, const char* what) {
	printf("%-52s %s\n", what, ok ? "OK" : "ERRO");
	if (!ok) fails++;
}
static bool same(const Frame& a, const Frame& b) {
	return a.type == b.type && a.seq == b.seq && a.len == b.len && !memcmp(a.payload, b.payload, a.len);
}
static Frame mk(uint8_t len, int mode) {  // mode 0 = aleatório, 1 = tudo zero, 2 = tudo 0xFF
	Frame f;
	f.type = (uint8_t)rnd();
	f.seq = (uint8_t)rnd();
	f.len = len;
	for (uint8_t i = 0; i < len; i++)
		f.payload[i] = mode == 1 ? 0 : mode == 2 ? 0xFF : (uint8_t)(rnd() % 3 ? rnd() : 0);
	return f;
}
static int feedAll(Parser& p, const uint8_t* b, size_t n, Frame& last) {
	int cnt = 0;
	for (size_t i = 0; i < n; i++)
		if (p.feed(b[i], last)) cnt++;
	return cnt;
}

int main() {
	const uint8_t v[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
	chk(crc16(v, 9) == 0x29B1, "CRC16 vetor '123456789' = 0x29B1");

	// 1) ida e volta, todos os tamanhos, com zeros e 0xFF
	bool ok = true;
	for (int it = 0; it < 3000 && ok; it++) {
		Frame f = mk(it % (MAX_PAYLOAD + 1), it % 7 == 0 ? 1 : it % 11 == 0 ? 2 : 0), r;
		uint8_t w[MAX_FRAME];
		size_t n = encode(f, w);
		if (!n || w[n - 1] != 0) ok = false;
		for (size_t i = 0; i + 1 < n; i++)
			if (w[i] == 0) ok = false;  // sem zero antes do delimitador
		Parser p;
		if (feedAll(p, w, n, r) != 1 || !same(f, r)) ok = false;
	}
	chk(ok, "ida e volta (3000 quadros, payload 0..8)");

	// 2) qualquer bit trocado é rejeitado e o parser se recupera
	ok = true;
	for (int it = 0; it < 200 && ok; it++) {
		Frame f = mk(it % (MAX_PAYLOAD + 1), 0), r;
		uint8_t w[MAX_FRAME];
		size_t n = encode(f, w);
		for (size_t i = 0; i + 1 < n && ok; i++)
			for (int b = 0; b < 8 && ok; b++) {
				uint8_t c[MAX_FRAME];
				memcpy(c, w, n);
				c[i] ^= (uint8_t)(1 << b);
				Parser p;
				if (feedAll(p, c, n, r) != 0) ok = false;
				if (feedAll(p, w, n, r) != 1 || !same(f, r)) ok = false;  // recuperou
			}
	}
	chk(ok, "1 bit trocado: rejeitado e recupera no proximo");

	// 3) lixo (com zeros) antes de um quadro bom
	ok = true;
	for (int it = 0; it < 500 && ok; it++) {
		Frame f = mk(1 + it % 8, 0), r = {};
		uint8_t w[MAX_FRAME], junk[40];
		size_t n = encode(f, w), jn = 1 + rnd() % 40;
		for (size_t i = 0; i < jn; i++) junk[i] = (uint8_t)rnd();
		Parser p;
		Frame tmp;
		feedAll(p, junk, jn, tmp);
		p.feed(0, tmp);  // fecha o que sobrou
		if (feedAll(p, w, n, r) != 1 || !same(f, r)) ok = false;
	}
	chk(ok, "ressincroniza depois de lixo");

	// 4) estouro de buffer
	{
		Parser p;
		Frame f = mk(5, 0), r;
		uint8_t w[MAX_FRAME], big[100];
		memset(big, 0x55, sizeof(big));
		Frame t;
		feedAll(p, big, sizeof(big), t);
		p.feed(0, t);
		size_t n = encode(f, w);
		chk(feedAll(p, w, n, r) == 1 && same(f, r), "estouro de buffer e recuperacao");
	}

	// 5) fuzz: bytes aleatórios não podem travar nem quebrar
	{
		Parser p;
		Frame t;
		int accepted = 0;
		for (int i = 0; i < 2000000; i++) accepted += p.feed((uint8_t)rnd(), t) ? 1 : 0;
		printf("fuzz: 2e6 bytes, aceitos=%d, descartados=%u\n", accepted, (unsigned)p.bad());
		chk(accepted < 100, "fuzz sem aceitar quase nada");
	}

	printf(fails ? "\nFALHOU (%d)\n" : "\nTUDO OK\n", fails);
	return fails ? 1 : 0;
}
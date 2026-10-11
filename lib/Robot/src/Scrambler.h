// Scrambler.h — embaralhamento com seed (xorshift32, igual ao do sweep) em JANELAS.
// Janela = 1 face, ou 2 faces OPOSTAS girando juntas. Sem dependência de hardware (roda no PC).
// Partindo do estado atual: gera só os giros; quem executa aplica no CubeState (applyMoves(str)).
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "CubeState.h"

struct ScrWindow {
	uint8_t faceA, turnsA;  // faceA sempre válida (0..5)
	uint8_t faceB, turnsB;  // faceB = 255 e turnsB = 0 => janela de face única
};

class Scrambler {
   public:
	explicit Scrambler(uint32_t seed = 1) { reseed(seed); }
	void reseed(uint32_t seed) { _x = seed ? seed : 0x9E3779B9u; }
	uint32_t next() {
		_x ^= _x << 13;
		_x ^= _x >> 17;
		_x ^= _x << 5;
		return _x;
	}

	/* Gera n janelas. Janelas seguidas nunca usam o mesmo eixo (senão se fundiriam em uma).
	 * Por eixo há 15 combinações (voltas A,B em 0..3, menos 0/0), sorteadas uniformemente. */
	uint8_t generate(ScrWindow* out, uint8_t n) {
		int8_t lastAxis = -1;
		for (uint8_t k = 0; k < n; k++) {
			uint8_t axis = lastAxis < 0 ? next() % 3 : (lastAxis + 1 + next() % 2) % 3;
			uint8_t combo = 1 + next() % 15;
			uint8_t ta = combo / 4, tb = combo % 4;
			ScrWindow w;
			if (ta && tb)
				w = {axis, ta, (uint8_t)(axis + 3), tb};
			else if (ta)
				w = {axis, ta, 255, 0};
			else
				w = {(uint8_t)(axis + 3), tb, 255, 0};
			out[k] = w;
			lastAxis = (int8_t)axis;
		}
		return n;
	}

	static uint8_t axisOf(const ScrWindow& w) { return w.faceA % 3; }

	/* "R L' U2 ..." (formato do CubeState::applyMoves / CubeView::moves). Retorna o tamanho ou -1. */
	static int toString(const ScrWindow* w, uint8_t n, char* out, size_t cap) {
		if (cap == 0) return -1;
		size_t p = 0;
		for (uint8_t k = 0; k < n; k++) {
			if (!put(w[k].faceA, w[k].turnsA, out, cap, p)) return -1;
			if (w[k].turnsB && !put(w[k].faceB, w[k].turnsB, out, cap, p)) return -1;
		}
		if (p) p--;  // tira o espaço final
		out[p] = 0;
		return (int)p;
	}

   private:
	uint32_t _x;
	static bool put(uint8_t face, uint8_t turns, char* out, size_t cap, size_t& p) {
		if (p + 4 > cap) return false;
		out[p++] = "URFDLB"[face];
		if (turns == 2)
			out[p++] = '2';
		else if (turns == 3)
			out[p++] = '\'';
		out[p++] = ' ';
		return true;
	}
};
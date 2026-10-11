// poses.h — GERADO por tools/pose_sim (nao edite a mao; rode o pose_sim de novo)
// Canonico: j=0..8 U, 9..17 F, 18..26 R; dentro da face: linha*3+coluna visto de fora.
// Todos os indices Kociemba abaixo se referem ao estado ANTES de executar T.
#pragma once
#include <stdint.h>

#define POSE_T_SEQ "D R2 L2 U D F B' R2 L'"
#define POSE_T_INV_SEQ "L R2 B F' D' U' L2 R2 D'"
#define POSE_N 27

// j -> facelet Kociemba mostrado na pose A
static const uint8_t POSE_A_FACELET[27] = {0,1,2,3,4,5,6,7,8,18,19,20,21,22,23,24,25,26,9,10,11,12,13,14,15,16,17};

// j -> facelet Kociemba (antes do T) mostrado na pose B
static const uint8_t POSE_B_FACELET[27] = {38,41,44,37,4,30,36,39,42,47,50,53,32,22,52,45,48,51,33,43,27,34,13,28,35,46,29};

// facelet movel -> pose que o mostra (0=A, 1=B, 255=centro)
static const uint8_t POSE_OF_FACELET[54] = {0,0,0,0,255,0,0,0,0,0,0,0,0,255,0,0,0,0,0,0,0,0,255,0,0,0,0,1,1,1,1,255,1,1,1,1,1,1,1,1,255,1,1,1,1,1,1,1,1,255,1,1,1,1};

// facelet movel -> j canonico na imagem dessa pose (255=centro)
static const uint8_t POSE_IMG_OF_FACELET[54] = {0,1,2,3,255,5,6,7,8,18,19,20,21,255,23,24,25,26,9,10,11,12,255,14,15,16,17,20,23,26,5,255,12,18,21,24,6,3,0,7,255,1,8,19,2,15,25,9,16,255,10,17,14,11};

static inline bool poseIsCenter(uint8_t j) { return (j % 9) == 4; }

// Orientação da câmera: rot = quartos de volta horários, mirror = espelho horizontal (aplicado antes).
// Converte o índice canônico j no índice da ordem de ROI da câmera (grade 3x3 de cada face).
struct CamOrient {
	uint8_t rot;
	bool mirror;
};
static inline uint8_t poseOrient(uint8_t j, CamOrient o) {
	uint8_t f = j / 9, r = (j % 9) / 3, c = j % 3;
	if (o.mirror) c = 2 - c;
	for (uint8_t k = 0; k < o.rot; k++) {  // 90 graus horario: (r,c) -> (c, 2-r)
		uint8_t nr = c, nc = 2 - r;
		r = nr;
		c = nc;
	}
	return f * 9 + r * 3 + c;
}

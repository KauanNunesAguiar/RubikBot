// pose_sim.cpp — valida a sequência T (troca de pose A->B), busca outra se preciso e gera poses.h
// Build (raiz do projeto):
//   g++ -O2 -std=c++17 -Ilib/CubeCore/src -Ilib/Robot/src tools/pose_sim/pose_sim.cpp lib/CubeCore/src/CubeState.cpp -o
//   pose_sim
// Uso: pose_sim [--seq "B F' L R' U D' F B'"] [--search] [--maxw 5] [--out lib/Robot/src/poses.h]
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "BenchConfig.h"  // só para os static_assert serem checados no PC
#include "CubeState.h"
#include "Scrambler.h"

static const uint8_t VIS[3] = {FACE_U, FACE_F, FACE_R};  // faces vistas pela câmera (canto URF)
static uint8_t P[27];                                    // j canônico -> facelet Kociemba (pose A)
static bool g_isVisMov[54];
static uint8_t g_visMov[24];
static bool isCenter(uint8_t j) { return j % 9 == 4; }

static std::string moveStr(uint8_t face, uint8_t turns) {
	std::string s(1, "URFDLB"[face]);
	if (turns == 2)
		s += '2';
	else if (turns == 3)
		s += '\'';
	return s;
}

static std::string invSeq(const std::string& seq) {
	std::vector<CubeMove> v;
	const char* p = seq.c_str();
	CubeMove m;
	while (CubeState::nextMove(p, m)) v.push_back(m);
	std::string out;
	for (size_t i = v.size(); i--;) {
		CubeMove q = CubeState::inverse(v[i]);
		if (!out.empty()) out += ' ';
		out += moveStr(q.face, q.turns);
	}
	return out;
}

/* ------------------------------ validação de T ------------------------------ */
struct PoseCheck {
	bool ok;
	uint8_t perm[54];  // perm[i] = onde foi parar o adesivo que começou em i
	uint8_t src[54];   // src[p] = de onde veio o adesivo que está em p
	uint8_t srcB[27];  // j -> facelet Kociemba (ANTES do T) que aparece na imagem da pose B
	char why[96];
};

static PoseCheck check(const std::string& seq) {
	PoseCheck r;
	memset(&r, 0, sizeof(r));
	if (!CubeState::stickerPerm(seq.c_str(), r.perm)) {
		snprintf(r.why, sizeof(r.why), "sequencia invalida");
		return r;
	}
	for (uint8_t i = 0; i < 54; i++) r.src[r.perm[i]] = i;
	for (uint8_t f = 0; f < 6; f++)
		if (r.perm[f * 9 + 4] != f * 9 + 4) {
			snprintf(r.why, sizeof(r.why), "um centro se moveu");
			return r;
		}
	int bad = 0;
	for (uint8_t j = 0; j < 27; j++) {
		r.srcB[j] = r.src[P[j]];
		if (!isCenter(j) && g_isVisMov[r.srcB[j]]) bad++;
	}
	if (bad) {
		snprintf(r.why, sizeof(r.why), "%d das 24 facetas moveis da pose B repetem facetas da pose A", bad);
		return r;
	}
	r.ok = true;
	return r;
}

/* ------------------------------ busca de T (IDDFS) ------------------------------ */
struct Win {
	uint8_t a, ta, tb;  // eixo 0..2 (faces a e a+3), voltas de cada uma (0..3)
	uint8_t perm[54];
};
static std::vector<Win> g_win;
static std::vector<int> g_cur, g_best;
static int g_target, g_bestCost;
static bool g_haveBest;

static std::string winStr(const Win& w) {
	std::string s;
	if (w.ta) s += moveStr(w.a, w.ta);
	if (w.tb) {
		if (!s.empty()) s += ' ';
		s += moveStr(w.a + 3, w.tb);
	}
	return s;
}
static int q(uint8_t t) { return t == 0 ? 0 : (t == 2 ? 2 : 1); }                 // custo em quartos de volta
static int wcost(const Win& w) { return q(w.ta) > q(w.tb) ? q(w.ta) : q(w.tb); }  // as duas giram juntas

static void buildWindows() {
	g_win.clear();
	for (uint8_t a = 0; a < 3; a++)
		for (uint8_t ta = 0; ta < 4; ta++)
			for (uint8_t tb = 0; tb < 4; tb++) {
				if (!ta && !tb) continue;
				Win w;
				w.a = a;
				w.ta = ta;
				w.tb = tb;
				CubeState::stickerPerm(winStr(w).c_str(), w.perm);
				g_win.push_back(w);
			}
}

static void dfs(int depth, int lastAxis, const uint8_t* perm, int cost) {
	const bool last = (depth + 1 == g_target);
	for (size_t k = 0; k < g_win.size(); k++) {
		const Win& w = g_win[k];
		if (w.a == lastAxis) continue;  // mesmo eixo seguido = uma janela só
		int c = cost + wcost(w);
		g_cur[depth] = (int)k;
		if (last) {
			if (g_haveBest && c >= g_bestCost) continue;
			bool ok = true;
			for (uint8_t n = 0; n < 24 && ok; n++)
				if (g_isVisMov[w.perm[perm[g_visMov[n]]]]) ok = false;
			if (ok) {
				g_haveBest = true;
				g_bestCost = c;
				g_best = g_cur;
			}
		} else {
			uint8_t np[54];
			for (uint8_t i = 0; i < 54; i++) np[i] = w.perm[perm[i]];
			dfs(depth + 1, w.a, np, c);
		}
	}
}

static std::string search(int maxw) {
	buildWindows();
	uint8_t id[54];
	for (uint8_t i = 0; i < 54; i++) id[i] = i;
	for (g_target = 1; g_target <= maxw; g_target++) {
		g_haveBest = false;
		g_bestCost = 0;
		g_cur.assign(g_target, 0);
		printf("busca com %d janela(s)...\n", g_target);
		fflush(stdout);
		dfs(0, -1, id, 0);
		if (g_haveBest) {
			std::string s;
			for (int k = 0; k < g_target; k++) {
				if (k) s += ' ';
				s += winStr(g_win[g_best[k]]);
			}
			printf("achou: %d janelas, custo %d quartos de volta\n", g_target, g_bestCost);
			return s;
		}
	}
	return "";
}

/* ------------------------------ teste do Scrambler ------------------------------ */
static bool testScrambler() {
	Scrambler s1(BENCH_SEED_TEST), s2(BENCH_SEED_TEST);
	bool ok = true;
	int singles = 0, pairs = 0;
	for (int n = 0; n < 2000; n++) {
		ScrWindow w1[BENCH_SCRAMBLE_WINDOWS], w2[BENCH_SCRAMBLE_WINDOWS];
		s1.generate(w1, BENCH_SCRAMBLE_WINDOWS);
		s2.generate(w2, BENCH_SCRAMBLE_WINDOWS);
		char t1[200], t2[200];
		if (Scrambler::toString(w1, BENCH_SCRAMBLE_WINDOWS, t1, sizeof(t1)) < 0) ok = false;
		if (Scrambler::toString(w2, BENCH_SCRAMBLE_WINDOWS, t2, sizeof(t2)) < 0) ok = false;
		if (strcmp(t1, t2) != 0) ok = false;  // mesma seed => mesma sequência
		CubeState c;
		if (!c.applyMoves(t1)) ok = false;  // parser aceita
		for (int k = 0; k < BENCH_SCRAMBLE_WINDOWS; k++) {
			if (k && Scrambler::axisOf(w1[k]) == Scrambler::axisOf(w1[k - 1])) ok = false;
			if (w1[k].turnsA < 1 || w1[k].turnsA > 3) ok = false;
			if (w1[k].turnsB) {
				pairs++;
				if (w1[k].faceB != w1[k].faceA + 3 || w1[k].turnsB > 3) ok = false;
			} else {
				singles++;
			}
		}
	}
	printf("Scrambler: %s (janelas simples %d, pares %d)\n", ok ? "OK" : "ERRO", singles, pairs);
	return ok;
}

/* ------------------------------ geração do poses.h ------------------------------ */
static void arr(FILE* f, const char* name, const uint8_t* a, int n, const char* doc) {
	fprintf(f, "// %s\nstatic const uint8_t %s[%d] = {", doc, name, n);
	for (int i = 0; i < n; i++) fprintf(f, "%s%u", i ? "," : "", (unsigned)a[i]);
	fprintf(f, "};\n\n");
}

static void emit(const char* path, const std::string& seq, const std::string& inv, const PoseCheck& r) {
	FILE* f = fopen(path, "w");
	if (!f) {
		printf("ERRO: nao abriu %s\n", path);
		return;
	}
	uint8_t of[54], img[54];
	memset(of, 255, sizeof(of));
	memset(img, 255, sizeof(img));
	for (uint8_t j = 0; j < 27; j++)
		if (!isCenter(j)) {
			of[P[j]] = 0;
			img[P[j]] = j;
			of[r.srcB[j]] = 1;
			img[r.srcB[j]] = j;
		}
	int cover = 0;
	for (uint8_t k = 0; k < 54; k++) cover += (of[k] != 255);
	printf("cobertura das facetas moveis: %d/48\n", cover);

	fprintf(f, "// poses.h — GERADO por tools/pose_sim (nao edite a mao; rode o pose_sim de novo)\n");
	fprintf(f, "// Canonico: j=0..8 U, 9..17 F, 18..26 R; dentro da face: linha*3+coluna visto de fora.\n");
	fprintf(f,
	        "// Todos os indices Kociemba abaixo se referem ao estado ANTES de executar T.\n#pragma once\n#include "
	        "<stdint.h>\n\n");
	fprintf(f, "#define POSE_T_SEQ \"%s\"\n#define POSE_T_INV_SEQ \"%s\"\n#define POSE_N 27\n\n", seq.c_str(),
	        inv.c_str());
	arr(f, "POSE_A_FACELET", P, 27, "j -> facelet Kociemba mostrado na pose A");
	arr(f, "POSE_B_FACELET", r.srcB, 27, "j -> facelet Kociemba (antes do T) mostrado na pose B");
	arr(f, "POSE_OF_FACELET", of, 54, "facelet movel -> pose que o mostra (0=A, 1=B, 255=centro)");
	arr(f, "POSE_IMG_OF_FACELET", img, 54, "facelet movel -> j canonico na imagem dessa pose (255=centro)");
	fputs(R"X(static inline bool poseIsCenter(uint8_t j) { return (j % 9) == 4; }

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
)X",
	      f);
	fclose(f);
	printf("gravado %s\n", path);
}

int main(int argc, char** argv) {
	std::string seq = "B F' L R' U D' F B'";
	bool force = false;
	int maxw = 5;
	const char* out = nullptr;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--seq") && i + 1 < argc)
			seq = argv[++i];
		else if (!strcmp(argv[i], "--search"))
			force = true;
		else if (!strcmp(argv[i], "--maxw") && i + 1 < argc)
			maxw = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--out") && i + 1 < argc)
			out = argv[++i];
	}

	CubeState::init();
	int nv = 0;
	for (uint8_t j = 0; j < 27; j++) {
		P[j] = VIS[j / 9] * 9 + j % 9;
		g_isVisMov[P[j]] = !isCenter(j);
		if (!isCenter(j)) g_visMov[nv++] = P[j];
	}

	bool scrOk = testScrambler();

	PoseCheck r = check(seq);
	printf("T = %s\n  -> %s%s\n", seq.c_str(), r.ok ? "VALIDA" : "INVALIDA: ", r.ok ? "" : r.why);
	if (!r.ok || force) {
		std::string s = search(maxw);
		if (s.empty()) {
			if (!r.ok) {
				printf("nenhuma T com ate %d janelas (tente --maxw %d)\n", maxw, maxw + 1);
				return 1;
			}
		} else {
			seq = s;
			r = check(seq);
			printf("T = %s\n", seq.c_str());
		}
	}
	if (!r.ok) return 1;

	std::string inv = invSeq(seq);
	uint8_t pp[54];
	CubeState::stickerPerm((seq + " " + inv).c_str(), pp);
	bool id = true;
	for (uint8_t i = 0; i < 54; i++)
		if (pp[i] != i) id = false;
	printf("T' = %s\nT seguido de T' = identidade: %s\n", inv.c_str(), id ? "OK" : "ERRO");

	printf("pose B (j: facelet Kociemba):");
	for (uint8_t j = 0; j < 27; j++) printf("%s%u", j % 9 ? " " : "\n  ", (unsigned)r.srcB[j]);
	printf("\n");

	if (out) emit(out, seq, inv, r);
	return (id && scrOk) ? 0 : 1;
}
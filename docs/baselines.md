# Dados medidos e experimentos descartados

Tudo aqui é histórico/referência (movido do TODO). Para comparar qualquer mudança de desempenho do solver, use o sweep (N >= 100), nunca 5 cubos.

## 1. Solver

**Referência atual** (sweep N=200, seed 2024, 30 giros, maxDepth 26, refine 1500 ms, `_p2Budget=30000`):
200/200 resolvidos · 21,72 giros em média (faixa 18..25) · média 3113 ms · mediana 2625 ms · p90 5403 ms · máx 7996 ms · >10 s: 0 · >7 s: 3.

Baselines com seed fixa 12345 (5 cubos, subestimam o caso típico):

| Baseline | Mudança                                                  | Média   | Observação                                   |
| -------- | -------------------------------------------------------- | ------- | -------------------------------------------- |
| 1        | escalonamento fase 2, filtro fase 1, `g_cnk`, SPI 42 MHz, -O2 | 2567 ms | 1ª solução 573..1673 ms; flash 34-35%        |
| 2        | corte antecipado `h1` em `search2` + `multiplyP2`        | 2300 ms | -10,4% vs baseline 1 (budget ilimitado)      |
| (CCM)    | `_cc` e `g_m18` na CCM RAM                               | ~3100 ms | com budget 30000; neutro (+0,2% no sweep)    |

Passos da Etapa 6B (selftest, 5 cubos): corte antecipado -6,5%; `multiplyP2` -4,1%; ordem `us`→`cs` sem ganho; `permRank` por popcount sem ganho (revertido).

### Varredura de `_p2Budget` (N=200, seed 2024, 200/200 em todos)

| budget    | média    | mediana | p90  | máx   | >10s | >7s | giros |
| --------- | -------- | ------- | ---- | ----- | ---- | --- | ----- |
| 0         | 3999     | 3150    | 7162 | 19119 | 9    | 20  | 22,17 |
| 300000    | 3934     | 3153    | 6853 | 18264 | 10   | 19  | 22,13 |
| 150000    | 3767     | 3073    | 6505 | 21357 | 7    | 16  | 21,99 |
| 80000     | 3477     | 2837    | 6186 | 14666 | 2    | 14  | 21,84 |
| 50000     | 3357     | 2818    | 5969 | 9821  | 0    | 11  | 21,74 |
| 37000     | 3227     | 2736    | 5748 | 9260  | 0    | 9   | 21,73 |
| **30000** | **3113** | 2625    | 5403 | 7996  | 0    | 3   | 21,72 |
| 22000     | 3158     | 2739    | 5253 | 8718  | 0    | 5   | 21,70 |
| 20000     | 3115     | 2784    | 4988 | 8310  | 0    | 5   | 21,68 |
| 10000     | 3158     | 2756    | 5151 | 11506 | 2    | 5   | 21,63 |

Fundo da curva entre 20000 e 30000 (-22% na média, máx 19119 → 7996 ms); 10000 corta G1 úteis e a cauda piora.
**Lição para o TCC:** N=5 deu conclusão errada (+34%); N=200 corrigiu.

### Perfil de uma busca

- ~69% CPU, ~31-35% flash externa (~2,4 us por leitura, SPI 42 MHz).
- F1 estável em ~1,1-2,4 s; F2 de 0,2 s a 16 s (cauda pesada). Sem budget, buscas de até 38 s.
- Eventos do solver (`SOLVER_EVENTS=1`): overhead +4,3% (2671 vs 2562 ms, mesmos 22 giros).
- Exemplo de 1 busca: F1 158243 nós / 146261 podas; F2 344540 nós / 294534 podas. Ramificação efetiva F1 ~3,14 (d10).
- Correção: o DWT (32 bits) dá a volta em 25,57 s a 168 MHz; `phase2` mede por limite, não por chamada. Tempos F1/F2 de buscas longas anteriores à correção são inválidos.

## 2. Render e UI

~21 a 34 fps; quadro médio ~10,3 ms (~4,6 ms em `pushColors`/FSMC). Troca de tela bloqueada durante o solve: OK.

## 3. Memória e pilhas

- `sizeof(CubieCube)` = 40 B; `_cc[33]` = 1320 B e `g_m18` = 720 B (CCM, +0x0000 e +0x0800). `sizeof(CubeSolver)` 1460 → 144 depois da CCM.
- Heap FreeRTOS 119488 B: canvas 80000 B (67%), livre 22716 B (mín = livre, sem alocação em runtime). .bss ~11,7 KB.
- Pilhas (words livres mín. / total): display 444/768 · solve 311/768 · dispatch 228/512 · input 114/192 · led 105/128 · metrics 236/512.
- Uso medido (words): display 324 · solve 471 · dispatch 284 · input 78 · led 23 · metrics 276. Economia de ~5,3 KB de heap ao reduzir as pilhas.

## 4. Descartados (não refazer sem motivo novo)

| Ideia                                  | Motivo                                                            |
| -------------------------------------- | ----------------------------------------------------------------- |
| Cache de poda em RAM                   | acerto simulado 9-15% (acessos muito dispersos)                   |
| Pré-filtro em RAM                      | sem ganho                                                         |
| `fs` antes de `ts`; `us` antes de `cs` | sem ganho (cortam os mesmos nós)                                  |
| `permRank` por máscara + popcount      | 2315 vs 2300 ms; rank não é o gargalo                             |
| Empacotar `CubieCube` (Experimento A)  | economiza ~1 KB, shift/máscara no caminho quente. Reabrir só p/ resultado negativo no TCC |
| Canvas do cubo em faixas               | sem pressão de RAM; custo de `setAddrWindow`/re-raster            |
| DFU para upload                        | não funcionou na bancada; trocado por SWD (ver TODO, etapa 9.0)   |

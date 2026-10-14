# O cubo na memória

O projeto usa **duas representações** do mesmo cubo, cada uma boa para uma coisa:

| Representação | Onde             | Tamanho | Usada por                                  |
| ------------- | ---------------- | ------- | ------------------------------------------ |
| `CubeState`   | `lib/CubeCore`   | 54 B    | tela (CubeView), HUD, entrada, protocolo   |
| `CubieCube`   | `lib/CubeCore`   | 40 B    | só o solver (e o gerador de tabelas no PC) |

`CubeState` diz **que cor está em cada adesivo**. `CubieCube` diz **onde está cada peça e como ela está virada**. O solver converte de uma para a outra (`fromFacelets`) uma vez por busca.

## 1. CubeState: 54 adesivos

Um vetor `_s[54]` de bytes. A ordem é a do Kociemba: **U R F D L B**, 9 adesivos por face.

```
              U 0..8
   L 36..44   F 18..26   R 9..17   B 45..53
              D 27..35
```

Dentro de cada face o índice é `linha*3 + coluna`, visto **de fora** da face (índice 4 = centro):

```
 0 1 2
 3 4 5
 6 7 8
```

**O valor guardado não é uma cor RGB**, é o ID da face de origem da cor (`FACE_U=0, FACE_R=1, FACE_F=2, FACE_D=3, FACE_L=4, FACE_B=5`). A cor na tela vem da paleta do `CubeView` (`_palette[id]`), que pode ser trocada sem tocar no estado.

Consequências:

- Cubo resolvido: `_s[f*9 + i] == f` para todo adesivo. `isSolved()` compara cada adesivo com o centro da própria face; funciona porque giros de face nunca movem centros.
- `get(face, idx)` / `set(face, idx, faceId)` acessam `_s[face*9 + idx]`.
- Formato texto (Kociemba): 54 letras `URFDLB` na mesma ordem. Resolvido:
  `UUUUUUUUURRRRRRRRRFFFFFFFFFDDDDDDDDDLLLLLLLLLBBBBBBBBB` (`toString` / `fromString`; `fromString` não verifica se é resolvível).

### Quem altera o CubeState

- Giros via `CubeView::move/moves` entram numa fila (64 posições). O `CubeState` **só muda quando o giro termina de animar** (`applyMove` dentro de `CubeView::update`). Durante a animação a tela desenha a camada girando por geometria, sem tocar no estado.
- O HUD tira uma cópia (`CubeState snap = _cs`) antes de chamar o solver, então a busca não é afetada por nada que aconteça na tela.

## 2. Geometria e geração das permutações

Cada face tem, em `CUBE_FACE_GEOM`, três vetores inteiros: `n` (normal, para fora), `u` (direção das colunas) e `v` (direção das linhas). Essa tabela é a **única** fonte de verdade: gera as permutações dos giros e também é usada no desenho 3D.

A posição de um adesivo (em unidades de meio-adesivo, o cubo vai de -3 a +3) é:

```
p = 3*n + 2*c*u + 2*r*v        com c = idx%3 - 1  e  r = idx/3 - 1   (c, r em {-1, 0, 1})
```

Os ciclos de cada giro **não são digitados à mão**. `initTables()` os gera uma vez:

1. Para cada face `m`, o eixo é `a = n` da face.
2. Um adesivo pertence à camada que gira se `dot(p, a) > 0`. Isso pega os 9 adesivos da própria face e as 3 tiras vizinhas de cada uma das 4 faces laterais.
3. Posição `p` e normal `n` desse adesivo giram -90° (horário visto de fora) em torno de `a`: `v' = -(a x v) + a*(a . v)`.
4. `findSticker(n', p')` procura qual adesivo está nesse destino.
5. `g_perm[m][i]` = destino do adesivo `i` depois de **um** giro horário (adesivos fora da camada ficam onde estão).

`applyMove(face, turns)` faz, `turns` vezes, `novo[g_perm[face][i]] = antigo[i]`. Ou seja: 90° horário = 1 vez, 180° = 2, anti-horário = 3.

### Notação de giros

`CubeMove {face, turns}`: `turns` 1 = horário (`R`), 2 = meia-volta (`R2`), 3 = anti-horário (`R'`).
`nextMove` aceita também `R3`, `R1` e `R2'`; separa por espaço/tab/quebra de linha e rejeita colados como `R2U` ou `RR`.
`applyMoves(seq)` valida a sequência inteira antes de aplicar: se houver token inválido, **não aplica nada**.

`stickerPerm(seq, perm)` compõe os giros e devolve `perm[i]` = posição final do adesivo que começou em `i`. É a base do `pose_sim` (ver [poses.md](poses.md)).

## 3. CubieCube: 8 cantos + 12 arestas

```c
struct CubieCube {
    uint8_t cp[8],  co[8];    // canto que está na posição i, e sua torção (0..2)
    uint8_t ep[12], eo[12];   // aresta que está na posição i, e sua inversão (0..1)
};
```

Numeração (posição e peça usam o mesmo índice):

| Cantos | 0 URF | 1 UFL | 2 ULB | 3 UBR | 4 DFR | 5 DLF | 6 DBL | 7 DRB |
| ------ | ----- | ----- | ----- | ----- | ----- | ----- | ----- | ----- |

| Arestas | 0 UR | 1 UF | 2 UL | 3 UB | 4 DR | 5 DF | 6 DL | 7 DB | 8 FR | 9 FL | 10 BL | 11 BR |
| ------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ----- | ----- |

As arestas 8..11 são as da **camada do meio** (slice FR, FL, BL, BR).

- `cp[i] = j`: na posição `i` está a peça `j`. `co[i]`: posição (0, 1 ou 2) do adesivo U/D dentro do canto.
- `ep[i] = j`: na posição `i` está a aresta `j`. `eo[i]`: 1 se estiver invertida.
- Estado resolvido: tudo identidade e orientação 0 (`reset()`).

### Invariantes (o que `verify()` confere)

Só 1 em 12 das combinações de peças é alcançável. Um cubo montado "errado" (adesivo descolado e recolado) viola uma destas regras:

| Código `verify()` | Regra violada                                          |
| ----------------- | ------------------------------------------------------ |
| `-2`              | alguma aresta repetida ou ausente                      |
| `-3`              | soma das inversões das arestas é ímpar                 |
| `-4`              | algum canto repetido ou ausente                        |
| `-5`              | soma das torções dos cantos não é múltiplo de 3        |
| `-6`              | paridade da permutação das arestas != a dos cantos     |

`fromFacelets` devolve `false` (e o solver `-1`) quando não consegue sequer identificar uma peça. Com isso o HUD mostra mensagens como "Canto torcido" (`verifyErr()`).
O total de estados alcançáveis é `8! * 3^7 * 12! * 2^11 / 12 = 43.252.003.274.489.856.000`.

### Facelets -> peças (`fromFacelets`)

As tabelas `CORNER_FACELET[8][3]` e `EDGE_FACELET[12][2]` dão os índices (0..53) dos adesivos de cada posição; `CORNER_COLOR` e `EDGE_COLOR` dão as cores de cada peça.

Exemplo, canto na posição 0 (URF): facelets `{8, 9, 20}` = U8, R0, F2.

- **Canto:** a torção `ori` é a posição, dentro da trinca, do adesivo cuja cor é U ou D. As outras duas cores (em ordem horária) identificam a peça `j` em `CORNER_COLOR`. Resultado: `cp[i] = j`, `co[i] = ori`.
- **Aresta:** o par de cores é comparado com `EDGE_COLOR[j]` direto (`eo = 0`) ou invertido (`eo = 1`).

`toFacelets` faz o caminho inverso (usa `co`/`eo` para rotacionar qual adesivo recebe qual cor).

### Multiplicação de peças (aplicar giros)

`a.multiply(b)` = "aplica `b` depois de `a`":

```
cp[i] = a.cp[b.cp[i]]          co[i] = (a.co[b.cp[i]] + b.co[i]) % 3
ep[i] = a.ep[b.ep[i]]          eo[i] = a.eo[b.ep[i]] ^ b.eo[i]
```

Versões reduzidas, usadas na busca:

| Função               | Atualiza         | Uso                                                      |
| -------------------- | ---------------- | -------------------------------------------------------- |
| `multiplyP1(a, b)`   | `co`, `eo`, `ep` | fase 1 (não precisa de `cp`)                             |
| `multiplyP2(a, b)`   | `cp`, `ep`       | fase 2 (`co`/`eo` já são todos 0 em G1)                  |

### Os giros em nível de peça

- `CubieCube::initMoves()` gera os 6 giros horários **aplicando o giro no `CubeState` resolvido e lendo o resultado com `fromFacelets`**. Assim as duas representações nunca discordam.
- `coordsInit()` monta os 18 giros: `g_m18[face*3 + (turns-1)]`, na ordem `U U2 U' R R2 R' F F2 F' D D2 D' L L2 L' B B2 B'` (U, R, F, D, L, B). É por isso que `face = m / 3` e `turns = m % 3 + 1` aparecem pelo código.

## 4. Onde ficam na RAM

- `g_m18` (18 x 40 = 720 B) e `_cc[33]` (1320 B, um `CubieCube` por profundidade da busca) ficam na **CCM RAM** (`0x10000000`): `_cc` em `+0x0000` e `g_m18` em `+0x0800`. O `static_assert` do `CubeSolver` garante que `_cc` não invade `g_m18`.
- A CCM só é acessível pela CPU (sem DMA) e, por isso, só pode existir **um** `CubeSolver` por firmware.
- No build do PC (`tools/pc`) `g_m18` é um vetor comum.

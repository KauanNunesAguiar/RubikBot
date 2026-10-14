# Como a solução é encontrada

`CubeSolver` implementa o algoritmo de **duas fases de Kociemba**, com busca em profundidade iterativa (IDA\*) guiada por tabelas de poda. Ele devolve a **primeira** solução que encontra (não a ótima) e, opcionalmente, gasta um tempo extra procurando uma mais curta.

## 1. A ideia das duas fases

O cubo tem ~4,3 x 10^19 estados: impossível buscar direto. Kociemba divide o problema usando o subgrupo **G1** = cubos que podem ser resolvidos só com `U, U2, U', D, D2, D', R2, L2, F2, B2`.

| Fase | Objetivo                                                                                  | Giros permitidos           |
| ---- | ----------------------------------------------------------------------------------------- | -------------------------- |
| 1    | levar o cubo a G1: cantos e arestas orientados (twist = 0, flip = 0) e as 4 arestas do meio na camada do meio (slice = 0) | todos os 18 |
| 2    | resolver o cubo de vez, sem sair de G1                                                    | 10 giros (`P2_MOVES`): `U U2 U' R2 F2 D D2 D' L2 B2` |

A solução final é: (giros da fase 1) + (giros da fase 2). Como a fase 2 só tem 10 giros e começa já "organizada", ela é muito mais barata que buscar o cubo inteiro.

## 2. Coordenadas

Cada aspecto do cubo vira um número inteiro pequeno (calculado em `Coords.cpp` a partir de um `CubieCube`):

| Coordenada  | O que representa                                           | Valores              | Fase |
| ----------- | ---------------------------------------------------------- | -------------------- | ---- |
| `twist`     | torção dos 8 cantos (base 3; o 8º é determinado)           | 3^7 = 2187           | 1    |
| `flip`      | inversão das 12 arestas (base 2; a 12ª é determinada)      | 2^11 = 2048          | 1    |
| `slice`     | **quais** 4 posições entre 12 têm arestas do meio (ordem não importa), numeração combinatória | C(12,4) = 495 | 1 |
| `cperm`     | permutação dos 8 cantos (código de Lehmer)                 | 8! = 40320           | 2    |
| `udperm`    | permutação das 8 arestas U/D (`ep[0..7]`)                  | 8! = 40320           | 2    |
| `sliceperm` | ordem das 4 arestas do meio dentro da camada do meio       | 4! = 24              | 2    |

Espaços de busca: fase 1 = 2187 x 2048 x 495 = 2.217.093.120 estados; fase 2 = 8! x 8! x 24 = 39.016.857.600 estados. Cada fase é bem menor que o cubo completo.

`slice = 0`, `twist = 0`, `flip = 0` significam "resolvido" para a fase 1; para a fase 2, `cperm = udperm = sliceperm = 0` significa cubo resolvido.

> **No Master não há tabelas de movimento.** A cada nó da busca o solver multiplica `CubieCube`s (`multiplyP1`/`multiplyP2`) e recalcula as coordenadas com `getTwist`, `getFlip`, etc. As tabelas de movimento (`buildMoveTable`) só existem no PC, para gerar as tabelas de poda. Evoluir isso para coordenadas incrementais está no checklist (18.14).

## 3. Tabelas de poda

Para cortar a busca cedo, cada tabela guarda, para um **par** de coordenadas, o menor número de giros necessário para levar esse par ao estado resolvido (um limite inferior do que falta no cubo real).

| Tabela        | Par                    | Fase | Giros usados |
| ------------- | ---------------------- | ---- | ------------ |
| `twist_slice` | twist x slice          | 1    | 18           |
| `flip_slice`  | flip x slice           | 1    | 18           |
| `cperm_sp`    | cperm x sliceperm      | 2    | 10           |
| `udperm_sp`   | udperm x sliceperm     | 2    | 10           |

A heurística de um nó é o **máximo** das tabelas da fase. Se `h > giros restantes`, o nó é cortado: não existe jeito de terminar a tempo. Como cada valor nunca superestima a distância real, a busca não perde soluções. Detalhes de geração e armazenamento: [prune_tables.md](prune_tables.md).

## 4. Fase 1: `search1(depth, remaining, lastFace)`

```
conta o nó e confere o timeout
se remaining == 0:                        # chegou ao fim desta profundidade
    se não está em G1                  -> falha
    se o último giro é U/D ou meia-volta -> falha   (a fase 2 faria igual: duplicaria soluções)
    refaz cp ao longo do caminho       # multiplyP1 não mantém cp
    return phase2(depth, lastFace)
se poda(twist,slice) > remaining ou poda(flip,slice) > remaining -> falha
para cada um dos 18 giros permitidos:
    c[depth+1] = c[depth] * giro       # multiplyP1
    se search1(depth+1, remaining-1, face) -> sucesso
```

- Profundidade iterativa: `d1 = 0, 1, 2, ...` (cada `d1` é uma chamada com `remaining = d1`).
- "Em G1" no código = arestas 8..11 são do meio (`ep[8..11] >= 8`), `co[0..6] == 0`, `eo[0..10] == 0` (o último canto/aresta é determinado pelos outros).
- Filtro de giros (`allowed`): nunca a mesma face duas vezes seguidas, e para faces opostas só uma ordem (U antes de D, R antes de L, F antes de B), pois `U D` e `D U` dão o mesmo cubo. Faces 0..5 = U R F D L B: opostas têm o mesmo `face % 3`.
- A cadeia `_cc[0..depth]` guarda um `CubieCube` por nível; ao voltar de um ramo é só sobrescrever o próximo.

## 5. Fase 2: `phase2` e `search2`

Cada vez que a fase 1 chega a G1 (evento "G1 alcançado"), `phase2` roda:

```
h = max(poda cperm_sp, poda udperm_sp)       # distância mínima possível
para lim = h, h+1, ... até _p2Max e (depth + lim <= maxDepth):
    search2(depth, lim, lastFace)            # IDA* da fase 2
    se achou -> solução = fase1 + fase2
```

`search2` é igual à fase 1, só que com os 10 giros de `P2_MOVES`, `multiplyP2` e as tabelas da fase 2. Duas otimizações medidas:

1. **Corte antecipado:** lê `cperm_sp` e poda **antes** de ler `udperm_sp` (cada leitura vai à flash externa).
2. `h == 0` nas duas tabelas = cubo resolvido: grava `_len = depth` e termina.

### Orçamento por chamada (`_p2Budget`, padrão 30000)

O tempo da fase 2 tem **cauda pesada**: alguns G1 levam milhares de vezes mais que a média. `_p2Budget` limita os nós gastos **numa chamada de `phase2`** (somando todos os `lim`). Estourou: a chamada é cortada (`_p2Cut`, métrica `p2Cuts`) e a fase 1 segue para o próximo G1. Varredura medida (N = 200 cubos): sem limite a média foi 3999 ms e o máximo 19119 ms; com 30000, média 3113 ms e máximo 7996 ms. Abaixo de ~20000 passa a cortar G1 úteis e a cauda piora.

## 6. Estágios (`runStages`)

A busca não vai direto ao teto de profundidade. Ela escalona o limite da fase 2 (`_p2Max`) para achar uma primeira solução rápido:

| Estágio | `_p2Max` (giros máx. na fase 2) |
| ------- | ------------------------------- |
| 0       | 10                              |
| 1       | 12                              |
| 2       | 14                              |
| 3       | 18                              |

Em cada estágio a fase 1 refaz `d1 = 0 ... lim1`, onde `lim1 = 12` nos estágios 0..2 (se `maxDepth > 12`) e `maxDepth` no estágio 3. Com `refineMs > 0` a **primeira** solução já começa no estágio 2 (`runStages(maxDepth, 2)`), porque o refinamento vai repetir a busca com tetos menores de qualquer forma.

## 7. Refinamento (`refineMs`)

Depois da primeira solução (tamanho `L`), o solver guarda a melhor e roda `runStages(L-1, 0)` repetidamente, com um novo cronômetro de `refineMs`, enquanto achar uma solução mais curta. Quando o tempo acaba (ou não existe solução menor) a busca para e devolve a melhor encontrada, **sem erro**. O HUD usa `refineMs = 1500`. Resultado típico: 21,7 giros em média (faixa 18..25).

## 8. Limites, erros e retorno

`int solve(state, out, outLen, maxDepth = 30, timeoutMs = 60000, refineMs = 0)`: devolve o número de giros e escreve `"R U2 F' ..."` em `out`.

| Retorno | Significado                                                           |
| ------- | --------------------------------------------------------------------- |
| `>= 0`  | nº de giros (0 = já resolvido)                                        |
| `-1`    | cubo inválido; `verifyError()` diz qual regra (ver `cube_state.md`)   |
| `-2`    | tempo esgotado sem nenhuma solução                                    |
| `-3`    | sem solução até `maxDepth`                                            |
| `-4`    | buffer de saída pequeno                                               |

O timeout é checado em `tick()` a cada 256 nós (`millis() - _t0 > _timeout`). O HUD chama `solve(snap, _sol, 128, 26, 60000, 1500)`.

## 9. Fluxo completo de um SOLVE

```
K1 / botão SOLVE -> HUD.requestSolve() (_solveReq = true, guarda o instante do clique)
task "solve" -> solveStep():
    copia o CubeState; solver.solve(...)
    erro  -> ST_ERROR com mensagem
    ok    -> cube.moves(solução) enfileira os giros; ST_SOLVING
task "display" -> CubeView anima cada giro e só então aplica no CubeState
HUD: cubo parado por 60 ms e isSolved() -> ST_SOLVED (cronômetro fecha)
```

Enquanto busca, `HudScreen` troca automaticamente para a `TreeScreen` (árvore ao vivo) e volta ~1,2 s depois do fim.

## 10. Medição e visualização

- `Metrics` (`lib/Board`): leituras de flash por tabela, tempo de flash (DWT), nós por fase, chamadas de G1, limites de fase 2, cortes por orçamento. Aparecem na `DebugScreen` e no dump serial do botão WKUP.
- `SolverEvents` (`-D SOLVER_EVENTS=1`): histograma de nós e podas por profundidade, caminho atual e anel de 64 eventos, base da `TreeScreen` e do CSV exportado. Overhead medido: +4,3%.

Números de referência (sweep N = 200, seed 2024, budget 30000, `checklist.txt`): 200/200 resolvidos, média 3113 ms, mediana 2625 ms, p90 5403 ms, máximo 7996 ms. Cerca de 69% do tempo é CPU e 31% leitura da flash externa (~2,4 us por leitura). Cache de poda em RAM foi descartado (acerto simulado de 9 a 15%: os acessos são muito dispersos).

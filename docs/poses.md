# Poses A e B

Fonte da verdade numérica: `lib/Robot/src/poses.h` (gerado por `tools/pc/pose_sim`). Este texto só explica.

## Conceito

- A câmera é fixa e vê o canto **URF**: faces **U, F, R** (24 facetas móveis + 3 centros = 27 posições).
- **Pose A** = cubo como está. **Pose B** = depois de executar a sequência **T** (`POSE_T_SEQ`).
- T usa só giros de pares de faces opostas, então os centros não se movem. Condição validada no PC:
  as 24 facetas móveis da pose B são todas diferentes das da pose A (A + B = as 48 móveis) e
  T seguido de T' (`POSE_T_INV_SEQ`) é a identidade.
- Centros: ficam parcialmente cobertos pelo conector do motor. Rótulo = `x` (ignorar);
  no RESULT o slave devolve 7 (desconhecida). Eles não entram na acurácia.
- T adotada: D R2 L2 U D F B' R2 L' (5 janelas, 7 quartos de volta); T' = L R2 B F' D' U' L2 R2 D'.

## Ordem canônica das 27 posições (j)

| j      | face | observação                                     |
| ------ | ---- | ---------------------------------------------- |
| 0..8   | U    | linha\*3+coluna, visto de fora (como Kociemba) |
| 9..17  | F    | idem                                           |
| 18..26 | R    | idem                                           |

Centros: j = 4, 13, 22.

## Mapas (todos referentes ao estado S ANTES do T)

- `POSE_A_FACELET[j]`: índice Kociemba (0..53) exibido na pose A (U→0..8, F→18..26, R→9..17).
- `POSE_B_FACELET[j]`: índice Kociemba (de S) da faceta que aparece na posição j depois do T.
  Calculado por simulação (`CubeState::stickerPerm`), nunca digitado.
- Rótulo: `label[j] = S[POSE_x_FACELET[j]]` (classe 0..5 = U,R,F,D,L,B, ordem do `CubeFace`).
- Reconstrução (13.2): para cada faceta móvel k, `POSE_OF_FACELET[k]` diz a pose e
  `POSE_IMG_OF_FACELET[k]` diz o j. Centros: o Master preenche com a própria face.
- Comparação: sempre contra S (estado antes do T). Se o Master já aplicou T no `CubeState`,
  compara contra `S' = T(S)` nos índices `POSE_A_FACELET` (equivalente).

## Orientação por câmera

Cada slave guarda a lista de ROI numa ordem própria (por face, grade 3x3). O Master envia
rótulos na ordem canônica; o slave converte com `poseOrient(j, {rot, mirror})`
(rot = quartos de volta horários, mirror = espelho horizontal aplicado antes).
A tabela fica no sidecar (`orient`) e no HELLO. Valores definidos na calibração das ROI (10.6).

## Regenerar

`build/pc/pose_sim.exe --out lib/Robot/src/poses.h` (usa `--search` para procurar outra T com menos janelas).

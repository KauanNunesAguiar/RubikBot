# Tabelas de poda (W25Q16)

Quatro tabelas de 4 bits por entrada, geradas no PC e gravadas na flash SPI externa W25Q16 (2 MB). O Master só **lê** (1 byte por consulta).

## 1. Conteúdo

Para um par de coordenadas `(a, b)`: `d[a * nB + b]` = menor número de giros que leva `(a, b)` a `(0, 0)`. Os valores cabem em 4 bits (0..15); o gerador aborta se passar de 15.

Empacotamento: 2 entradas por byte. **Índice par = nibble baixo, índice ímpar = nibble alto.**

```c
uint8_t PruneTable::get(uint32_t idx) {
    uint8_t b = flash->read8(base + (idx >> 1));
    return (idx & 1) ? (b >> 4) : (b & 0x0F);
}
```

## 2. Geração (`tools/pc/gen_tables/gen.cpp`)

1. `coordsInit()` e uma **tabela de movimento** para cada coordenada: `table[coord * nMoves + k]` = coordenada depois do giro `k` (`buildMoveTable`: monta um cubo só com aquela coordenada, aplica o giro e lê a coordenada de volta).
2. `buildPrune` faz uma **busca em largura** a partir de `(0, 0)`: marca profundidade 0; para cada profundidade, expande todos os pares dessa profundidade pelos giros e marca os novos com `profundidade + 1`. Termina quando nenhum par novo aparece. Imprime quantos pares novos surgiram em cada profundidade.
3. `save` empacota 2 entradas por byte e grava o `.bin` **na pasta atual**.

Como a BFS parte do estado resolvido e os giros são reversíveis, a distância encontrada é a mesma que a do estado até o resolvido.

| Arquivo             | Pares         | Giros | Tabelas de movimento usadas       |
| ------------------- | ------------- | ----- | --------------------------------- |
| `twist_slice.bin`   | twist x slice | 18    | twist, slice                      |
| `flip_slice.bin`    | flip x slice  | 18    | flip, slice                       |
| `cperm_sp.bin`      | cperm x sp    | 10    | cperm, sliceperm (`P2_MOVES`)     |
| `udperm_sp.bin`     | udperm x sp   | 10    | udperm, sliceperm (`P2_MOVES`)    |

Build: `tools/pc/build.ps1` (gera `build/pc/gen_tables.exe`).

## 3. Layout na flash

| Tabela        | Entradas  | Bytes   | Início     | Termina em (exclusivo) |
| ------------- | --------- | ------- | ---------- | ---------------------- |
| `twist_slice` | 1.082.565 | 541.283 | `0x000000` | `0x084263`             |
| `flip_slice`  | 1.013.760 | 506.880 | `0x085000` | `0x100C00`             |
| `cperm_sp`    | 967.680   | 483.840 | `0x101000` | `0x177200`             |
| `udperm_sp`   | 967.680   | 483.840 | `0x178000` | `0x1EE200`             |

Total: 2.015.843 bytes. Os inícios são múltiplos de 4 KB (o setor mínimo de apagamento). Os endereços vivem em `PruneTable.h` (`PRUNE_ADDR_*`) e **também** em `tools/py/flash_tables.py`; se mudar um, mude o outro (checklist 18.9 propõe gerar um header único).

## 4. Como o Master lê

`W25Q16::read8` vai direto aos registradores do SPI (comando `0x03` + endereço de 24 bits + 1 byte, CS por `BSRR`), ~2,4 us por leitura a 42 MHz. No `begin()` a classe descobre qual SPI está ligado (PB3/PB4/PB5 podem ser SPI1 ou SPI3) e compara o caminho rápido com o lento em 5 endereços; se divergir, cai no caminho seguro da biblioteca SPI.

> **Atenção:** uma tabela apagada lê `0xFF`, ou seja, poda 15 em tudo. O solver continua funcionando, mas corta quase todo o espaço e a busca pode ir até o timeout (60 s) sem aviso. O autoteste `poda` (`-D RUN_SELFTESTS=1`) detecta isso; a assinatura das tabelas no boot está planejada no checklist (18.9).

## 5. Gravação na flash (env `flasher` + `tools/py/flash_tables.py`)

Protocolo serial simples (921600 baud):

| Comando                                  | Resposta                                              |
| ---------------------------------------- | ----------------------------------------------------- |
| `'I'`                                    | 3 bytes do ID (`EF 40 15`)                            |
| `'W'` + `addr` (4 B LE) + `len` (4 B LE) | `'K'` se `addr % 4096 == 0` e `addr + len <= 0x200000`, senão `'E'` |

Depois do `'W'`, o PC envia blocos de 256 bytes. Para cada bloco a placa apaga o setor (se estiver em fronteira de 4 KB), grava a página e responde `'K'`. No fim devolve o **CRC32** (4 B LE) do conteúdo **lido de volta** da flash; o script compara com `zlib.crc32` do arquivo.

Passo a passo:

1. `tools/pc/build.ps1` e execute `build/pc/gen_tables.exe` dentro de `tables/` (ele grava na pasta atual; o script Python lê de `tables/`).
2. `pio run -e flasher -t upload`.
3. `python tools/py/flash_tables.py` (confira `PORT` e `BAUD`; a abertura da porta pode resetar a placa).
4. Volte para o env `master` e faça o upload normal.

## 6. Espaço livre

Depois de `0x1EE200` sobram ~72 KB. O checklist reserva `0x1EF000..0x1FEFFF` para o diário de retomada (18.10) e `0x1FF000` para a assinatura das tabelas (18.9).

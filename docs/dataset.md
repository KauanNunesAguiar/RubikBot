# Formato do dataset e dos resultados

## Identificadores

- `camera`: esp32cam | xiao_s3 | jl407_ov7670 | nano33_ov7675 | webcam
- `cube_id`: c1..c5 (cadastro em `cubes.csv`: cube_id, modelo, tom, superfície)
- `trial_id`: `t` + 6 dígitos (t000123). `pose`: A ou B. `light`: L0..L2 (+ lux medido no sidecar).
- Mapeamento classe→face (constante única): 0=U 1=R 2=F 3=D 4=L 5=B; 7 = desconhecida (só RESULT).

## Arquivos (um cartão SD por slave, sem SD compartilhado)

- /<camera>/<cube_id>/<trial_id><pose>.raw imagem SEM compressão, sem cabeçalho, linha a linha
- /<camera>/<cube_id>/<trial_id><pose>.json sidecar

`.raw`: RGB565 little-endian (ou YUYV, conforme `format` no sidecar); tamanho = largura*altura*2.

## Sidecar (JSON, uma linha por chave simples)

| chave                      | conteúdo                                                            |
| -------------------------- | ------------------------------------------------------------------- |
| trial_id, pose, session_id | identificação                                                       |
| camera, sensor, mcu        | origem (sensor lido do chip no HELLO)                               |
| cube_id                    | cubo                                                                |
| width, height, format      | resolução e formato (rgb565 / yuyv)                                 |
| regs                       | registradores fixos (exposição, ganho, balanço de branco) em hex    |
| light, lux                 | nível de luz e lux medido (se houver BH1750)                        |
| seed                       | seed do embaralhamento do trial                                     |
| labels                     | 27 chars na ordem canônica (docs/poses.md): '0'..'5'; centros = 'x' |
| orient                     | rot (0..3) e mirror (0/1) da câmera                                 |
| fw_hash, model_hash        | firmware e modelo                                                   |
| cam_pos                    | posição da câmera no quadrado latino (14.2)                         |
| flags                      | bitmask de qualidade (lote suspeito, controle, etc.)                |

## CSV de resultados (um por slave; offline no SD do slave, ao vivo no SD do Master)

Colunas, nesta ordem:
`trial_id,pose,attempt,session_id,camera,cube_id,light,lux,mcu,model_hash,truth,pred,conf_avg,t_read_us,t_pre_us,t_inf_us,t_total_us,ram_peak,flags`

- `truth` e `pred`: 27 chars canônicos; `pred` usa 0..5 ou 7; centros = 'x'.
- Ao vivo (Etapa 16) acrescenta: `t_trig_ack_ms,t_trig_result_ms,rtt_ms,timeouts,plan_ver,fw_ver`.

## Chaves de junção

- Slave ↔ Master: `trial_id` + `pose` + `attempt` (+ `session_id` ao vivo).
- Slave ↔ medidor de corrente: `batch_id` (linha serial do lote) + pulsos de marcação.
- Database v1: `manifest` com hash por arquivo; split por `trial_id` (nunca por imagem/faceta).

## Convenções

- Divisão treino/teste e folds por cubo ficam congelados em `split.csv` (trial_id, set, fold).
- Acurácia: 48 facetas móveis (24 por pose); reportar também por foto (24) e estado completo (48/48).

Poses, T e mapas de rótulos: poses.md.

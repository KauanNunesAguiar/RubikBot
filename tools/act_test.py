# act_test.py — testa o atuador pela serial (precisa de pyserial)
# Uso: python act_test.py COM11 ping | en 0|1 | ms N | light N | estop
#      python act_test.py COM11 move FA TA [FB TB] [dup]     (faces 0..5 = U R F D L B; voltas 1..3)
import sys, time, serial, struct

def crc16(b):
    c = 0xFFFF
    for x in b:
        c ^= x << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def cobs_enc(d):
    out = bytearray([0])
    idx = 0
    code = 1
    for b in d:
        if b == 0:
            out[idx] = code
            idx = len(out)
            out.append(0)
            code = 1
        else:
            out.append(b)
            code += 1
            if code == 0xFF:
                out[idx] = code
                idx = len(out)
                out.append(0)
                code = 1
    out[idx] = code
    return bytes(out)


def cobs_dec(d):
    out = bytearray()
    i = 0
    while i < len(d):
        code = d[i]
        i += 1
        if code == 0:
            return None
        for _ in range(code - 1):
            if i >= len(d) or d[i] == 0:
                return None
            out.append(d[i])
            i += 1
        if code != 0xFF and i < len(d):
            out.append(0)
    return bytes(out)


T = dict(
    ping=0x01, move=0x10, en=0x12, light=0x13, estop=0x14, ms=0x15, jog=0x16, speed=0x17
)
STATE = ["IDLE", "MOVING", "ESTOP", "DISABLED", "WDOG"]
RES = ["OK", "ESTOP", "DISABLED", "BUSY", "BAD_ARGS"]
seq = 0


def send(s, t, pl=b""):
    global seq
    raw = bytes([t, seq & 255, len(pl)]) + pl
    seq += 1
    c = crc16(raw)
    s.write(cobs_enc(raw + bytes([c >> 8, c & 255])) + b"\0")


def recv(s, timeout):
    t0 = time.time()
    buf = bytearray()
    while time.time() - t0 < timeout:
        b = s.read(1)
        if not b:
            continue
        if b[0] != 0:
            buf += b
            continue
        raw = cobs_dec(bytes(buf)) if buf else None
        buf = bytearray()
        if (
            raw
            and len(raw) >= 5
            and len(raw) == 5 + raw[2]
            and crc16(raw[:-2]) == (raw[-2] << 8 | raw[-1])
        ):
            return raw[0], raw[3 : 3 + raw[2]]
    return None


def show(r):
    if r is None:
        print("  sem resposta")
        return
    t, p = r
    if t == 0x02:
        print(
            f"  STATUS estado={STATE[p[0]] if p[0] < 5 else p[0]} ultimo_id={p[1]} err={p[2]}"
        )
    elif t == 0x11:
        print(f"  DONE id={p[0]} resultado={RES[p[1]] if p[1] < 5 else p[1]}")
    else:
        print("  tipo", hex(t), p.hex())


s = serial.Serial(sys.argv[1], 115200, timeout=0.05)
time.sleep(0.5)
s.reset_input_buffer()
cmd = sys.argv[2]
a = [int(x) for x in sys.argv[3:] if x != "dup"]

if cmd == "move":
    send(s, T["en"], bytes([1]))
    show(recv(s, 1))
    mid = int(time.time()) % 250 + 1
    fb, tb = (a[2], a[3]) if len(a) >= 4 else (255, 0)
    pl = bytes([mid, a[0], a[1], fb, tb])
    t0 = time.time()
    send(s, T["move"], pl)
    r = recv(s, 8)
    show(r)
    print(f"  tempo ate o DONE: {time.time() - t0:.3f} s")
    if "dup" in sys.argv:
        print("reenviando o MESMO id (nao pode girar):")
        t0 = time.time()
        send(s, T["move"], pl)
        show(recv(s, 2))
        print(f"  {time.time() - t0:.3f} s")
        
elif cmd == "jog":  # jog AXIS STEPS   (STEPS negativo = sentido contrário; passos crus no microstep atual)
    send(s, T["en"], bytes([1]))
    recv(s, 1)
    mid = int(time.time()) % 250 + 1
    t0 = time.time()
    send(s, T["jog"], bytes([mid, a[0]]) + struct.pack("<h", a[1]))
    show(recv(s, 8))
    print(f"  tempo ate o DONE: {time.time() - t0:.3f} s")
    
elif cmd == "speed":  # speed HZ   (50..4000, referido ao microstep 1/4)
    send(s, T["speed"], struct.pack("<H", a[0]))
    show(recv(s, 1))
    
else:
    send(s, T[cmd], bytes(a))
    show(recv(s, 1))

# Original 8-bit style UI sounds for kid mode, generated here (no samples from anywhere else).
# Rounded "bubble" voicing: sine with a little triangle, soft attack and an exponential fade,
# so nothing clicks or buzzes. Usage: python3 make-sounds.py <out dir>
import math, os, struct, sys, wave

SR = 44100

def note(freq, ms, vol):
    n = int(SR * ms / 1000)
    out = []
    ph = 0.0
    for i in range(n):
        ph += freq / SR
        p = ph % 1
        v = 0.8 * math.sin(2 * math.pi * p) + 0.2 * (4 * abs(p - 0.5) - 1)
        attack = min(1.0, i / (SR * 0.006))
        fade = math.exp(-3.5 * i / n) * min(1.0, (n - i) / (SR * 0.004))
        out.append(v * vol * attack * fade)
    return out

def rest(ms):
    return [0.0] * int(SR * ms / 1000)

def save(name, s):
    os.makedirs(sys.argv[1], exist_ok=True)
    with wave.open(os.path.join(sys.argv[1], name + '.wav'), 'w') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<hh', int(x * 32767), int(x * 32767)) for x in s))

C4, E4, G4 = 261.63, 329.63, 392.0
C5, D5, E5, G5, C6 = 523.25, 587.33, 659.25, 783.99, 1046.5

# Left/Right: two-note bubble (Eric's pick), 10% louder than the first version.
save('navigate', note(C5, 30, 0.11) + note(G5, 35, 0.088))
save('keypress', note(G5, 30, 0.10))
save('option', note(D5, 40, 0.11))
save('confirm', note(C5, 50, 0.18) + note(G5, 80, 0.18))
save('back', note(G5, 50, 0.16) + note(C5, 70, 0.16))
save('error', note(C4, 100, 0.20) + rest(30) + note(C4, 130, 0.20))
save('info_open', note(C5, 45, 0.14) + note(E5, 65, 0.14))
save('info_close', note(E5, 45, 0.14) + note(C5, 65, 0.14))
save('startup', note(C5, 100, 0.18) + note(E5, 100, 0.18) + note(G5, 100, 0.18) + note(C6, 320, 0.18))
save('shutdown', note(C6, 110, 0.17) + note(G5, 110, 0.17) + note(E5, 110, 0.17) + note(C5, 340, 0.17))
save('reboot', note(G5, 90, 0.17) + note(C5, 90, 0.17) + note(G5, 90, 0.17) + note(C6, 240, 0.17))
save('muos', rest(10))
print('ok')

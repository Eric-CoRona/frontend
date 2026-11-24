# Original 8-bit style UI sounds for kid mode (square/triangle waves, generated here).
import wave, struct, math, sys, os

SR=44100
def tone(freq,ms,vol=0.25,wave_='sq',duty=0.5,slide=0.0):
    n=int(SR*ms/1000); out=[]; ph=0.0
    for i in range(n):
        f=freq+(slide*i/n)
        ph+=f/SR; p=ph%1
        if wave_=='sq': v=1.0 if p<duty else -1.0
        else: v=4*abs(p-0.5)-1
        env=min(1,i/(SR*0.003))*min(1,(n-i)/(SR*0.012))
        out.append(v*vol*env)
    return out
def rest(ms): return [0.0]*int(SR*ms/1000)
def save(name,s):
    os.makedirs(sys.argv[1],exist_ok=True)
    with wave.open(os.path.join(sys.argv[1],name+'.wav'),'w') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b''.join(struct.pack('<hh',int(x*32767),int(x*32767)) for x in s))
N={'C5':523.25,'E5':659.25,'G5':783.99,'C6':1046.5,'A4':440,'F4':349.23,'D5':587.33,'B4':493.88,'G4':392.0,'E4':329.63,'C4':261.63}
def soft(freq,ms,vol,wave_='tri'):
    n=int(SR*ms/1000); out=[]; ph=0.0
    for i in range(n):
        ph+=freq/SR; p=ph%1
        v=(4*abs(p-0.5)-1) if wave_=='tri' else math.sin(2*math.pi*p)
        out.append(v*vol*min(1,i/(SR*0.006))*math.exp(-4.0*i/n))
    return out
# Navigate: soft two-note "bubble" (Eric's pick, 2026-11-23)
save('navigate',soft(523.25,30,0.10,'sine')+soft(783.99,35,0.08,'sine'))
save('keypress',tone(N['G5'],25,0.15,duty=0.25))
save('option',tone(N['D5'],35,0.18,duty=0.25))
save('confirm',tone(N['C5'],45,0.22)+tone(N['G5'],70,0.22))
save('back',tone(N['G5'],40,0.2)+tone(N['C5'],60,0.2))
save('error',tone(N['C4'],90,0.22,duty=0.5)+rest(30)+tone(N['C4'],120,0.22,duty=0.5))
save('info_open',tone(N['C5'],40,0.18,'tri')+tone(N['E5'],60,0.18,'tri'))
save('info_close',tone(N['E5'],40,0.18,'tri')+tone(N['C5'],60,0.18,'tri'))
save('startup',tone(N['C5'],90,0.22)+tone(N['E5'],90,0.22)+tone(N['G5'],90,0.22)+tone(N['C6'],260,0.22))
save('shutdown',tone(N['C6'],100,0.2)+tone(N['G5'],100,0.2)+tone(N['E5'],100,0.2)+tone(N['C5'],300,0.2,'tri'))
save('reboot',tone(N['G5'],80,0.2)+tone(N['C5'],80,0.2)+tone(N['G5'],80,0.2)+tone(N['C6'],200,0.2))
save('muos',rest(10))
print('ok')

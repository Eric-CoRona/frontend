from PIL import Image, ImageDraw
F = {
'T':["11111","00100","00100","00100","00100","00100","00100"],
'H':["10001","10001","10001","11111","10001","10001","10001"],
'E':["11111","10000","10000","11110","10000","10000","11111"],
'O':["01110","10001","10001","10001","10001","10001","01110"],
'S':["01111","10000","10000","01110","00001","00001","11110"],
'G':["01110","10001","10000","10111","10001","10001","01111"],
'A':["01110","10001","10001","11111","10001","10001","10001"],
'M':["10001","11011","10101","10101","10001","10001","10001"],
'B':["11110","10001","10001","11110","10001","10001","11110"],
'Y':["10001","10001","01010","00100","00100","00100","00100"],
"'":["1","1","0","0","0","0","0"],
'U':["10001","10001","10001","10001","10001","10001","01110"],
'N':["10001","11001","10101","10011","10001","10001","10001"],
'!':["1","1","1","1","1","0","1"],
' ':["000","000","000","000","000","000","000"],
}
W,H=720,480
def glyph_w(ch): return len(F[ch][0])
def text_w(t,px): return sum((glyph_w(c)+1)*px for c in t)-px
def draw_text(d,t,x,y,px,fill,shadow,off):
    for pass_ in (0,1):
        cx=x
        for ch in t:
            for r,row in enumerate(F[ch]):
                for c,v in enumerate(row):
                    if v=='1':
                        X=cx+c*px; Y=y+r*px
                        if pass_==0: d.rectangle([X+off,Y+off,X+off+px-1,Y+off+px-1],fill=shadow)
                        else: d.rectangle([X,Y,X+px-1,Y+px-1],fill=fill)
            cx+=(glyph_w(ch)+1)*px
im=Image.new('RGB',(W,H)); d=ImageDraw.Draw(im)
top,bot=(10,40,90),(8,90,120)
for y in range(H):
    t=y/(H-1); d.line([(0,y),(W,y)],fill=tuple(int(top[k]+(bot[k]-top[k])*t) for k in range(3)))
GREEN,BLUE,SH=(120,230,80),(140,210,255),(10,30,40)
# soft radial vignette toward the centre
import math
px_=im.load()
for y in range(H):
    for x in range(W):
        dx=(x-W/2)/(W/2); dy=(y-H/2)/(H/2); v=min(1,math.sqrt(dx*dx+dy*dy))
        r,g,b=px_[x,y]; k=1-0.35*v*v
        px_[x,y]=(int(r*k),int(g*k),int(b*k))
# faint pixel dot grid
for y in range(8,H,16):
    for x in range(8,W,16):
        r,g,b=px_[x,y]; c=(min(255,r+14),min(255,g+18),min(255,b+22))
        d.rectangle([x,y,x+1,y+1],fill=c)
# very subtle scanlines
for y in range(0,H,3):
    for x in range(W):
        r,g,b=px_[x,y]; px_[x,y]=(int(r*.93),int(g*.93),int(b*.93))
import sys
t1=sys.argv[1] if len(sys.argv)>1 else "THEO'S"
t2=sys.argv[2] if len(sys.argv)>2 else "GAMEBOY"  # "" for a single framed line
OUTFILE=sys.argv[3] if len(sys.argv)>3 else 'bootlogo.png'
p1=max([p for p in range(6,17) if text_w(t1,p)<=520] or [6])
p2=min(9,max([p for p in range(4,10) if text_w(t2,p)<=480] or [4])) if t2 else 1
w1,w2=text_w(t1,p1),text_w(t2,p2)
h1,h2=7*p1,7*p2
gap=44
if not t2: h2=0; gap=0
total=h1+gap+h2
y1=(H-total)//2-10; y2=y1+h1+gap
x1=(W-w1)//2; x2=(W-w2)//2
# outline: union of two padded boxes, drawn on a coarse pixel grid
G=4; pad=40
boxA=(x1-pad, y1-pad, x1+w1+pad, y1+h1+gap//2)
boxB=(x2-pad, y1+h1+gap//2, x2+w2+pad, y2+h2+pad)
if not t2: boxA=(x1-pad, y1-pad, x1+w1+pad, y1+h1+pad); boxB=boxA
def inside(px,py):
    for (a,b,c,e) in (boxA,boxB):
        if a<=px<c and b<=py<e: return True
    return False
cols,rows=W//G,H//G
cells=[[inside(cx*G+G//2,cy*G+G//2) for cx in range(cols)] for cy in range(rows)]
def border(cx,cy):
    if not cells[cy][cx]: return False
    for dx,dy in ((1,0),(-1,0),(0,1),(0,-1)):
        nx,ny=cx+dx,cy+dy
        if not(0<=nx<cols and 0<=ny<rows) or not cells[ny][nx]: return True
    return False
OUT=(70,150,125)
for cy in range(rows):
    for cx in range(cols):
        if cells[cy][cx]:
            for yy in range(cy*G,cy*G+G):
                for xx in range(cx*G,cx*G+G):
                    r,g,b=px_[xx,yy]; px_[xx,yy]=(int(r*.78),int(g*.82),int(b*.85))
for layer,(col,off) in enumerate(((SH,3),(OUT,0))):
    for cy in range(rows):
        for cx in range(cols):
            if border(cx,cy):
                X,Y=cx*G+off,cy*G+off
                d.rectangle([X,Y,X+G-1,Y+G-1],fill=col)
# inner blue line, inset one cell
for cy in range(1,rows-1):
    for cx in range(1,cols-1):
        if cells[cy][cx] and not border(cx,cy):
            if any(border(cx+dx,cy+dy) for dx,dy in ((1,0),(-1,0),(0,1),(0,-1),(1,1),(-1,-1),(1,-1),(-1,1))):
                pass
draw_text(d,t1,x1,y1,p1,GREEN,SH,6)
if t2: draw_text(d,t2,x2,y2,p2,BLUE,SH,4)
dy_=boxB[3]+34; s_=8
for i,col in enumerate((GREEN,OUT,BLUE)):
    X=W//2-(3*s_+2*s_)//2+i*2*s_
    d.rectangle([X+2,dy_+2,X+s_+1,dy_+s_+1],fill=SH); d.rectangle([X,dy_,X+s_-1,dy_+s_-1],fill=col)
im.save(OUTFILE)

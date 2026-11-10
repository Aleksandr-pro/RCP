from pathlib import Path
import math

for name in ('sand', 'ground', 'stone'):
    data = (Path('ATLAS/app/src/main/assets/textures') / f'{name}.rgba').read_bytes()
    assert len(data) == 512 * 512 * 4
    assert all(v == 255 for v in data[3::4])
    def pixel(x,y):
        i=(y*512+x)*4
        return data[i:i+3]
    h=[abs(a-b) for y in range(512) for a,b in zip(pixel(0,y),pixel(511,y))]
    v=[abs(a-b) for x in range(512) for a,b in zip(pixel(x,0),pixel(x,511))]
    # Compare boundary pixel change to ordinary neighbouring pixels in the tile.
    inner_h=[abs(a-b) for y in range(0,512,4) for x in range(0,511,4) for a,b in zip(pixel(x,y),pixel(x+1,y))]
    inner_v=[abs(a-b) for y in range(0,511,4) for x in range(0,512,4) for a,b in zip(pixel(x,y),pixel(x,y+1))]
    assert sum(h)/len(h) < 2.0 * (sum(inner_h)/len(inner_h)) + 1
    assert sum(v)/len(v) < 2.0 * (sum(inner_v)/len(inner_v)) + 1
    print(name, 'seam mean channel difference', sum(h)/len(h), sum(v)/len(v))

def smooth(a,b,x):
    t=max(0.,min(1.,(x-a)/(b-a)))
    return t*t*(3-2*t)
for i in range(-1000,1001):
    h=i/1000
    a=smooth(-.30,-.10,h); b=smooth(.25,.45,h)
    weights=(1-a,a-b,b)
    assert abs(sum(weights)-1)<1e-12 and min(weights)>=0
assert (1-smooth(-.30,-.10,-.3),smooth(-.30,-.10,-.3),smooth(.25,.45,-.3))==(1,0,0)
assert smooth(-.30,-.10,0)==1 and smooth(.25,.45,0)==0
assert smooth(.25,.45,.45)==1
print('Texture lengths, opacity, edge continuity and weight ranges passed')

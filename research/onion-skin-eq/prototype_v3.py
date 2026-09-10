"""Historical V3 heuristic reconstruction. Synthetic-only; not a masking model."""
import numpy as np
import pandas as pd

SR=48000; DURATION=8.0; NFFT=2048; HOP=512
t=np.arange(int(SR*DURATION))/SR

def tone(f,a=1): return a*np.sin(2*np.pi*f*t)
def pulse(bpm=120,width=.05,offset=0):
    return np.exp(-((((t-offset)%(60/bpm))/width)**2))

scenarios={
'Kick+Bass_coexist':{'Kick':.95*pulse(width=.045)*tone(60)+.15*pulse(width=.02)*tone(2800),'Bass':.58*(tone(72)+.35*tone(144)+.15*tone(288))},
'Guitars_compete':{'Guitar A':.45*(tone(420)+.45*tone(840)+.22*tone(1680)),'Guitar B':.43*(tone(460)+.48*tone(920)+.20*tone(1840))},
'Vocal_over_guitar':{'Vocal':(t>2)*(.65+.2*np.sin(2*np.pi*.35*t))*(.34*tone(210)+.28*tone(700)+.27*tone(1400)+.2*tone(2600)),'Guitar':.52*(.16*tone(220)+.20*tone(440)+.22*tone(880)+.18*tone(1760)+.14*tone(2640))},
'Separated_sources':{'Sub':.65*tone(55),'Air':.35*tone(9000)},
'Brief_collision':{'Vocal':.4*(tone(240)+.25*tone(1200)+.22*tone(2800)),'Cymbal':.55*pulse(60,.025,1.5)*(tone(3500)+.7*tone(7000)+.35*tone(11000))}}
F=np.fft.rfftfreq(NFFT,1/SR); valid=(F>=20)&(F<=16000); f=F[valid]
edges=np.array([20,60,100,150,200,300,400,510,630,770,920,1080,1270,1480,1720,2000,2320,2700,3150,3700,4400,5300,6400,7700,9500,12000,16000.])
centers=np.sqrt(edges[:-1]*edges[1:]); dt=HOP/SR

def spectrum(x):
    return np.array([np.abs(np.fft.rfft(x[i:i+NFFT]*np.hanning(NFFT)))[valid]**2 for i in range(0,len(x)-NFFT+1,HOP)]).T

def bands(M):
    return np.array([M[(f>=lo)&(f<hi)].mean(axis=0) if np.any((f>=lo)&(f<hi)) else np.zeros(M.shape[1]) for lo,hi in zip(edges[:-1],edges[1:])])

def occupancy(B):
    db=10*np.log10(B+1e-14); ref=np.percentile(db,97)
    return np.clip((db-(ref-38))/38,0,1)

def episodes(mask):
    runs=[]; counts=[]
    for row in mask:
        best=cur=count=0; prev=False
        for v in row:
            if v:
                cur+=1; best=max(best,cur)
                if not prev: count+=1
            else: cur=0
            prev=bool(v)
        runs.append(best*dt); counts.append(count)
    return np.array(runs),np.array(counts)

def analyze(stems):
    names=list(stems); A=occupancy(bands(spectrum(stems[names[0]]))); B=occupancy(bands(spectrum(stems[names[1]])))
    overlap=np.minimum(A,B); weighted=overlap*(1-np.abs(A-B))*(.55+.45*np.exp(-.5*((np.log10(centers)-np.log10(1200))/.9)**2))[:,None]
    run,counts=episodes(weighted>.32); continuity=np.clip((run-.05)/.45,0,1); repetition=np.clip(counts/8,0,1)
    asym=np.zeros(len(centers)); direction=np.zeros(len(centers))
    for i in range(len(centers)):
        active=overlap[i]>.2
        if active.any():
            diff=(A[i]-B[i])[active]; direction[i]=diff.mean(); asym[i]=np.clip(np.mean(np.abs(diff))/.35,0,1)
    severity=weighted.max(axis=1)*(.20+.55*continuity+.20*asym+.05*repetition)
    i=int(np.argmax(severity)); score=float(severity[i]); duration=float(run[i])
    label='Coexistence' if score<.18 else 'Pressure' if duration<.10 else 'Competition' if score<.48 or duration<.30 else 'Masking'
    dominance='Balanced' if abs(direction[i])<.08 else names[0] if direction[i]>0 else names[1]
    return dict(Pair=' ↔ '.join(names),Class=label,Severity=round(score,3),**{'Longest collision (ms)':round(duration*1000,1),'Episode count':int(counts[i]),'Hotspot Hz':round(float(centers[i]),1),'Asymmetry':round(float(asym[i]),3),'Dominance':dominance})

def run():
    return pd.DataFrame([dict(Scenario=name,**analyze(stems)) for name,stems in scenarios.items()])

if __name__=='__main__': print(run().to_string(index=False))

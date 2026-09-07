"""Monkey's Ear EQ V9 projected auditory-harmonic simulation.
Synthetic engineering research only; not perceptual validation.
"""
import numpy as np
import pandas as pd

rng=np.random.default_rng(910)
F0=110.0; N=14
freqs=F0*np.arange(1,N+1)
BASE_A=np.array([1,.80,.61,.47,.36,.29,.24,.19,.16,.13,.105,.085,.069,.056],float)
BASE_B=np.array([1,.63,.73,.35,.46,.24,.32,.17,.22,.13,.15,.09,.11,.065],float)
centers=np.geomspace(50,16000,96)

def erb_bw(f):
    return 24.7*(4.37*(np.asarray(f)/1000)+1)

def excitation(amps):
    E=np.zeros_like(centers)
    for a,f in zip(amps,freqs):
        bw=erb_bw(f)
        z=(centers-f)/(.60*bw+1e-12)
        E += a*a*np.exp(-.5*z*z)
    return np.sqrt(E+1e-15)

def conflict(a,b):
    ea,eb=excitation(a),excitation(b)
    return float(np.mean(np.minimum(ea,eb)/(np.maximum(ea,eb)+1e-12)))

def cosine(a,b):
    return float(np.dot(a,b)/(np.linalg.norm(a)*np.linalg.norm(b)+1e-12))

def energy_delta(a,b,a0,b0):
    return float(10*np.log10((np.sum(a*a)+np.sum(b*b)+1e-15)/(np.sum(a0*a0)+np.sum(b0*b0)+1e-15)))

def projected_auditory(a0,b0,sev,priority,max_db=3.0,step_db=.25):
    A=a0.copy(); B=b0.copy()
    preserved=A if priority>0 else B
    yielded=B if priority>0 else A
    yielded0=b0 if priority>0 else a0
    base_conf=conflict(A,B); current=base_conf
    max_steps=int((max_db*sev)/step_db)
    accepted=0

    Ep=excitation(preserved); Ey=excitation(yielded)
    comp=np.minimum(Ep,Ey)/(np.maximum(Ep,Ey)+1e-12)
    scores=[]
    for n,f in enumerate(freqs):
        if n<4:
            continue
        w=np.exp(-.5*((centers-f)/(.65*erb_bw(f)+1e-12))**2)
        local=float(np.sum(w*comp)/(np.sum(w)+1e-12))
        characteristic=yielded[n]/(yielded[n]+preserved[n]+1e-12)
        scores.append((local*(1-.7*characteristic),n))
    order=[n for _,n in sorted(scores,reverse=True)]

    for _ in range(max_steps):
        best=None
        for n in order:
            trial=yielded.copy()
            trial[n]*=10**(-step_db/20)
            if cosine(trial,yielded0)<.9975:
                continue
            if cosine(trial[:4],yielded0[:4])<.9999:
                continue
            c=conflict(preserved,trial) if priority>0 else conflict(trial,preserved)
            improvement=current-c
            removed=abs(10*np.log10((np.sum(trial**2)+1e-15)/(np.sum(yielded**2)+1e-15)))
            utility=improvement-.0025*removed
            if improvement>1e-6 and (best is None or utility>best[0]):
                best=(utility,n,trial,c)
        if best is None:
            break
        _,_,trial,current=best
        yielded[:] = trial
        accepted += 1
    return A,B,accepted,base_conf,current

rows=[]
for case in range(2000):
    sev=float(rng.beta(2,2)); pr=int(rng.choice([-1,1]))
    A0=BASE_A*np.exp(rng.normal(0,.09,N)); B0=BASE_B*np.exp(rng.normal(0,.09,N))
    A0/=A0[0]; B0/=B0[0]
    A,B,accepted,before,after=projected_auditory(A0,B0,sev,pr)
    yielded=B if pr>0 else A; yielded0=B0 if pr>0 else A0
    rows.append({
        'case':case,
        'severity':sev,
        'accepted_steps':accepted,
        'improvement_pct':100*(before-after)/(before+1e-12),
        'yielded_identity':cosine(yielded,yielded0),
        'yielded_low_identity':cosine(yielded[:4],yielded0[:4]),
        'energy_delta_db':energy_delta(A,B,A0,B0),
    })

df=pd.DataFrame(rows)
summary=pd.DataFrame([{
    'strategy':'Projected auditory harmonic',
    'mean_improvement_pct':df.improvement_pct.mean(),
    'p10_improvement_pct':df.improvement_pct.quantile(.10),
    'p01_improvement_pct':df.improvement_pct.quantile(.01),
    'zero_or_better_pct':100*np.mean(df.improvement_pct>=-1e-9),
    'mean_accepted_steps':df.accepted_steps.mean(),
    'p01_identity':df.yielded_identity.quantile(.01),
    'min_low_identity':df.yielded_low_identity.min(),
    'mean_energy_delta_db':df.energy_delta_db.mean(),
    'p05_energy_delta_db':df.energy_delta_db.quantile(.05),
}])
summary.to_csv('v9_projected_summary.csv',index=False)
print(summary.to_string(index=False))
assert summary['p01_improvement_pct'].iloc[0] >= -1e-8
assert summary['p01_identity'].iloc[0] >= .99749
assert summary['min_low_identity'].iloc[0] >= .999899

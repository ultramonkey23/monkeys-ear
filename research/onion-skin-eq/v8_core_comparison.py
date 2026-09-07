"""
Monkey's Ear EQ V8 core comparison.
Synthetic engineering research only; not perceptual validation.

Compares:
1) broad static carve,
2) conventional dynamic EQ,
3) harmonic-family gain control,
4) relational state dynamics.

The metrics are engineering proxies. They must not be called masking,
clarity, depth, or preference scores without real audio/listening validation.
"""
import numpy as np
import pandas as pd

SR=48000
CTRL=200
DUR=8.0
dt=1/CTRL
ct=np.arange(int(DUR*CTRL))/CTRL
A0=np.array([1.00,.78,.60,.46,.34,.28,.22,.18,.14,.11,.09,.07],float)
B0=np.array([1.00,.64,.72,.34,.45,.24,.31,.16,.21,.12,.15,.08],float)
N=len(A0)

severity=np.zeros_like(ct)
severity[(ct>=1.0)&(ct<2.5)] = .45
severity[(ct>=2.5)&(ct<3.0)] = .95
severity[(ct>=3.0)&(ct<5.5)] = .78
severity[(ct>=5.5)&(ct<6.5)] = .35
transient=np.zeros_like(ct)
transient[(ct>=2.5)&(ct<2.62)] = 1.0
priority=np.zeros_like(ct)
priority[(ct>=1.0)&(ct<4.0)] = 1.0
priority[(ct>=4.0)&(ct<6.5)] = -1.0

def cosine(a,b):
    return float(np.dot(a,b)/(np.linalg.norm(a)*np.linalg.norm(b)+1e-12))

def collision(a,b):
    an=a/(A0.max()+1e-12); bn=b/(B0.max()+1e-12)
    return float(np.mean(np.minimum(an,bn)))

def onepole(prev,target,tau_ms):
    alpha=1-np.exp(-dt/(tau_ms/1000))
    return prev+alpha*(target-prev)

def smooth_series(target, attack=50, release=180, slew_db_s=30):
    y=np.zeros_like(target,float)
    maxstep=slew_db_s*dt
    for i in range(1,len(target)):
        tau=attack if target[i] < y[i-1] else release
        cand=onepole(y[i-1], target[i], tau)
        y[i]=y[i-1]+np.clip(cand-y[i-1],-maxstep,maxstep)
    return y

A_broad=np.tile(A0,(len(ct),1)); B_broad=np.tile(B0,(len(ct),1))
for i,p in enumerate(priority):
    if p>0: B_broad[i,3:9] *= 10**(-3/20)
    elif p<0: A_broad[i,3:9] *= 10**(-3/20)

A_dyn=np.tile(A0,(len(ct),1)); B_dyn=np.tile(B0,(len(ct),1))
gr=-3.0*severity
for n in range(3,9):
    g=smooth_series(gr, attack=25, release=140, slew_db_s=40)
    for i,p in enumerate(priority):
        if p>0: B_dyn[i,n]*=10**(g[i]/20)
        elif p<0: A_dyn[i,n]*=10**(g[i]/20)

An=A0/A0.max(); Bn=B0/B0.max()
ownershipA=An/(An+Bn+1e-9); ownershipB=1-ownershipA
A_h=np.tile(A0,(len(ct),1)); B_h=np.tile(B0,(len(ct),1))
for n in range(2,N):
    conflict=np.minimum(An[n],Bn[n])
    target=-2.5*severity*conflict
    g=smooth_series(target,attack=45,release=220,slew_db_s=20)
    for i,p in enumerate(priority):
        if p>0: B_h[i,n]*=10**((g[i] if ownershipA[n]>=ownershipB[n] else .5*g[i])/20)
        elif p<0: A_h[i,n]*=10**((g[i] if ownershipB[n]>=ownershipA[n] else .5*g[i])/20)

A_rel=np.tile(A0,(len(ct),1)); B_rel=np.tile(B0,(len(ct),1))
persist=np.zeros_like(severity)
for i in range(1,len(ct)):
    persist[i]=onepole(persist[i-1],severity[i],140)
for n in range(2,N):
    conflict=np.minimum(An[n],Bn[n])
    base=-3.0*(persist**1.25)*conflict*(1-0.75*transient)
    g=smooth_series(base,attack=80,release=260,slew_db_s=10)
    for i,p in enumerate(priority):
        if p>0:
            B_rel[i,n]*=10**((g[i]*(.35+.65*ownershipA[n]))/20)
        elif p<0:
            A_rel[i,n]*=10**((g[i]*(.35+.65*ownershipB[n]))/20)

strategies={
    "Broad static carve":(A_broad,B_broad),
    "Conventional dynamic EQ":(A_dyn,B_dyn),
    "Harmonic-family control":(A_h,B_h),
    "Relational state dynamics":(A_rel,B_rel),
}
rows=[]
for name,(AA,BB) in strategies.items():
    collisions=[]; ida=[]; idb=[]; energy=[]
    motion=0.0
    for i in range(len(ct)):
        collisions.append(collision(AA[i],BB[i]))
        ida.append(cosine(AA[i],A0)); idb.append(cosine(BB[i],B0))
        e0=np.sum(A0**2)+np.sum(B0**2); e=np.sum(AA[i]**2)+np.sum(BB[i]**2)
        energy.append(10*np.log10((e+1e-12)/(e0+1e-12)))
        if i:
            motion += np.mean(np.abs(20*np.log10((AA[i]+1e-12)/(AA[i-1]+1e-12))))
            motion += np.mean(np.abs(20*np.log10((BB[i]+1e-12)/(BB[i-1]+1e-12))))
    rows.append({
        "strategy":name,
        "mean_collision":np.mean(collisions),
        "mean_A_identity":np.mean(ida),
        "mean_B_identity":np.mean(idb),
        "mean_energy_delta_dB":np.mean(energy),
        "control_motion_index":motion/DUR,
    })
df=pd.DataFrame(rows)
base_collision=collision(A0,B0)
df["collision_improvement_pct"]=(base_collision-df["mean_collision"])/base_collision*100
df["identity_floor"]=df[["mean_A_identity","mean_B_identity"]].min(axis=1)
df.to_csv("v8_core_comparison_scored.csv",index=False)
print(df.sort_values("mean_collision").to_string(index=False))

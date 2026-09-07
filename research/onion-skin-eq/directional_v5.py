"""V5 analysis-only experiment. No calibrated masking or automatic EQ."""
import numpy as np
import pandas as pd

def directional_evidence(target, masker, floor_db=-90.0):
    target=np.asarray(target,dtype=float); masker=np.asarray(masker,dtype=float)
    if target.shape!=masker.shape or np.any(target<0) or np.any(masker<0):
        raise ValueError('Matching nonnegative band-power arrays required')
    td=10*np.log10(np.maximum(target,1e-15)); md=10*np.log10(np.maximum(masker,1e-15))
    active=(td>floor_db)&(md>floor_db)
    margin=np.where(active,md-td,np.nan)
    return {'active':bool(np.any(active)),'max_masker_minus_target_db':float(np.nanmax(margin)) if np.any(active) else None}

def envelope_evidence(a,b):
    a=np.asarray(a,float); b=np.asarray(b,float)
    if a.shape!=b.shape: raise ValueError('Shapes differ')
    da=np.diff(a); db=np.diff(b); moving=(np.abs(da)+np.abs(db))>1e-6
    return float(np.mean(da[moving]*db[moving]<0)) if moving.any() else None

def run():
    cases=[('Equal energy',1.,1.),('Masker +12 dB',1.,10**(12/10)),('Masker -12 dB',1.,10**(-12/10)),('Both -20 dB',.01,.01),('Silent target',0.,1.),('Silent masker',1.,0.)]
    return pd.DataFrame([{'Case':name,**directional_evidence([a],[b])} for name,a,b in cases])

def test():
    df=run()
    assert df.loc[0,'max_masker_minus_target_db']==0
    assert abs(df.loc[1,'max_masker_minus_target_db']-12)<1e-10
    assert abs(df.loc[2,'max_masker_minus_target_db']+12)<1e-10
    assert not df.loc[4,'active'] and not df.loc[5,'active']
    assert envelope_evidence([1,2,3],[3,2,1])==1.
    assert envelope_evidence([1,2,3],[1,2,3])==0.

if __name__=='__main__':
    test(); print(run().to_string(index=False)); print('PASS')

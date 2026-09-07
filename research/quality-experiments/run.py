"""Standalone synthetic DSP experiments. No product DSP or audio-thread code.
Requires numpy, scipy, pandas. Run: python run.py
"""
import numpy as np
import pandas as pd
from scipy.signal import resample_poly
from pathlib import Path
out=Path(__file__).resolve().parent
fs=48000
t=np.arange(fs)/fs

def cubic(x,p):
    i=np.floor(p).astype(int); u=p-i
    a,b,c,d=[x[i+k] for k in (-1,0,1,2)]
    return b+.5*u*(c-a+u*(2*a-5*b+4*c-d+u*(3*(b-c)+d-a)))

def interpolation():
    rows=[]
    for f in (1000,6000,12000,18000):
        n=np.arange(-32,fs+33); x=np.sin(2*np.pi*f*n/fs)
        p=np.arange(fs)+32+.37; i=np.floor(p).astype(int); u=p-i
        truth=np.sin(2*np.pi*f*(np.arange(fs)+.37)/fs)
        offsets=np.arange(-16,17); d=p[:,None]-(i[:,None]+offsets)
        w=np.sinc(d)*np.sinc(d/17)
        methods={'linear':(1-u)*x[i]+u*x[i+1],'cubic':cubic(x,p),'sinc33':np.sum(x[i[:,None]+offsets]*w,axis=1)/np.sum(w,axis=1)}
        for name,y in methods.items():
            rows.append(dict(frequency_hz=f,method=name,rmse=float(np.sqrt(np.mean((y-truth)**2)))))
    return pd.DataFrame(rows)

def dither():
    rng=np.random.default_rng(20260907); step=2**-7
    x=.2*np.sin(2*np.pi*997*t)
    q=lambda z:step*np.floor(z/step+.5)
    rows=[]
    for name,e in (('undithered',q(x)-x),('TPDF',q(x+(rng.random(len(x))-rng.random(len(x)))*step)-x)):
        rows.append(dict(method=name,error_rms=float(np.sqrt(np.mean(e**2))),error_mean=float(np.mean(e)),input_error_correlation=float(np.corrcoef(x,e)[0,1])))
    return pd.DataFrame(rows)

def ada(x):
    F=np.logaddexp(x,-x)-np.log(2); prev=np.roll(x,1)
    d=x-prev; fp=np.roll(F,1)
    return np.where(abs(d)>1e-5,(F-fp)/np.where(abs(d)>1e-5,d,1),np.tanh((x+prev)/2))

def up(x,k):
    p=256
    return resample_poly(np.r_[x[-p:],x,x[:p]],k,1)[p*k:-p*k]

def down(x,k):
    p=256*k
    return resample_poly(np.r_[x[-p:],x,x[:p]],1,k)[256:-256]

def reference(f):
    phase=2*np.pi*np.arange(65536)/65536
    c=np.fft.rfft(np.tanh(3*np.sin(phase)))/65536
    h=np.arange(1,65536//2); h=h[h*f<fs/2]
    return np.sum(2*np.real(c[h,None]*np.exp(2j*np.pi*h[:,None]*f*t[None,:])),axis=0)

def nonlinear():
    rows=[]
    for f in (997,5999,11999):
        x=3*np.sin(2*np.pi*f*t); ref=reference(f)
        methods={'native':np.tanh(x),'ADAA1':ada(x),'2x':down(np.tanh(up(x,2)),2),'4x':down(np.tanh(up(x,4)),4),'ADAA1+2x':down(ada(up(x,2)),2)}
        for name,y in methods.items():
            delay=.5 if name=='ADAA1' else .25 if name=='ADAA1+2x' else 0
            if delay:
                y=np.fft.irfft(np.fft.rfft(y)*np.exp(2j*np.pi*np.fft.rfftfreq(len(y))*delay),n=len(y))
            rows.append(dict(frequency_hz=f,method=name,reference_rmse=float(np.sqrt(np.mean((y-ref)**2)))))
    return pd.DataFrame(rows)

if __name__=='__main__':
    for name,fn in (('interpolation',interpolation),('dither',dither),('nonlinear',nonlinear)):
        fn().to_csv(out/(name+'.csv'),index=False)
    print('PASS: three synthetic experiments written')

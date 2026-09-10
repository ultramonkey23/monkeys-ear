"""Monkey's Ear vocal resynthesis research v1.

Evidence state: SYNTHETIC.
Compares two transformation families on deterministic synthetic vowels and
compares practical LPC / cepstral spectral-envelope estimates against a known
oracle envelope. This is research evidence, not production or listening proof.

Requires: numpy, scipy, pandas, librosa.
"""
import numpy as np
import pandas as pd
from pathlib import Path
from scipy.signal import freqz
from scipy.linalg import toeplitz
from scipy.io import wavfile
import librosa

FS=48000
DUR=1.2
T=np.arange(int(FS*DUR))/FS
OUT=Path(__file__).resolve().parent
VOWELS={
    "a":[(730,90,1.00),(1090,110,.72),(2440,180,.45)],
    "i":[(270,70,1.00),(2290,140,.65),(3010,180,.42)],
    "u":[(300,80,1.00),(870,100,.72),(2240,170,.40)],
}

def env_curve(f, formants):
    f=np.asarray(f,float)
    y=np.full_like(f,.015)
    for fc,bw,g in formants:
        y += g/(1+((f-fc)/(bw/2))**2)
    return y/np.sqrt(np.maximum(f,80)/80)

def synth_vowel(f0, formants):
    h=np.arange(1,int((FS/2-500)//f0)+1)
    freqs=h*f0
    amps=env_curve(freqs,formants)
    phases=(h*.37)%(2*np.pi)
    y=np.sum(amps[:,None]*np.sin(2*np.pi*freqs[:,None]*T+phases[:,None]),axis=0)
    n=int(.03*FS); ramp=np.linspace(0,1,n)
    y[:n]*=ramp; y[-n:]*=ramp[::-1]
    return y/(max(np.max(np.abs(y)),1e-12)*1.05)

def psola_shift(x,f0,ratio):
    """Minimal known-F0 PSOLA-style baseline, not production PSOLA."""
    pin=FS/f0; pout=FS/(f0*ratio)
    src=np.arange(int(2*pin),len(x)-int(2*pin),pin)
    tgt=np.arange(int(2*pout),len(x)-int(2*pout),pout)
    out=np.zeros_like(x); norm=np.zeros_like(x)
    for tm in tgt:
        sm=src[np.argmin(np.abs(src-tm))]
        half=max(16,int(round(pin)))
        si,ti=int(round(sm)),int(round(tm))
        a0,a1=si-half,si+half+1; b0,b1=ti-half,ti+half+1
        if min(a0,b0)<0 or a1>len(x) or b1>len(x):
            continue
        w=np.hanning(a1-a0)
        out[b0:b1]+=x[a0:a1]*w; norm[b0:b1]+=w
    m=norm>1e-6
    out[m]/=norm[m]; out[~m]=x[~m]
    return out

def phase_vocoder_shift(x,semitones):
    return librosa.effects.pitch_shift(x.astype(float),sr=FS,n_steps=semitones,bins_per_octave=12)

def mag_db(x,nfft=65536):
    seg=x[int(.25*FS):int(.95*FS)]
    X=np.fft.rfft(seg*np.hanning(len(seg)),n=nfft)
    f=np.fft.rfftfreq(nfft,1/FS)
    return f,20*np.log10(np.maximum(np.abs(X),1e-10))

def smooth_db(f,db,width_hz=120):
    lin=10**(db/20); n=max(3,int(width_hz/(f[1]-f[0]))|1)
    sm=np.convolve(lin,np.ones(n)/n,mode="same")
    return 20*np.log10(np.maximum(sm,1e-12))

def spectral_rmse_db(y,reference):
    f,a=mag_db(y); _,b=mag_db(reference)
    a,b=smooth_db(f,a),smooth_db(f,b)
    m=(f>=100)&(f<=8000)
    da=a[m]-np.mean(a[m]); db=b[m]-np.mean(b[m])
    return float(np.sqrt(np.mean((da-db)**2)))

def formant_peak_error(y,formants):
    f,db=mag_db(y); sm=smooth_db(f,db,180); errors=[]
    for fc,_,_ in formants:
        m=(f>=max(80,fc-250))&(f<=fc+250)
        errors.append(abs(f[m][np.argmax(sm[m])]-fc))
    return float(np.mean(errors))

def lpc_envelope(x,order=20,nfft=16384):
    x=x-np.mean(x)
    r=np.correlate(x,x,mode="full")[len(x)-1:len(x)+order]
    R=toeplitz(r[:-1])
    coeff=np.linalg.solve(R+1e-8*np.eye(order),-r[1:])
    f,h=freqz([1.0],np.r_[1.0,coeff],worN=nfft//2+1,fs=FS)
    return f,20*np.log10(np.maximum(np.abs(h),1e-12))

def cepstral_envelope(x,lifter=40,nfft=16384):
    mag=np.maximum(np.abs(np.fft.rfft(x*np.hanning(len(x)),nfft)),1e-12)
    cep=np.fft.irfft(np.log(mag),nfft); kept=np.zeros_like(cep)
    kept[:lifter+1]=cep[:lifter+1]; kept[-lifter:]=cep[-lifter:]
    env=np.exp(np.fft.rfft(kept,nfft).real)
    f=np.fft.rfftfreq(nfft,1/FS)
    return f,20*np.log10(np.maximum(env,1e-12))

def run_resynthesis():
    rows=[]
    for vowel,formants in VOWELS.items():
        for f0 in (120.0,220.0):
            source=synth_vowel(f0,formants)
            for semitones in (-5,5,12):
                ratio=2**(semitones/12); oracle=synth_vowel(f0*ratio,formants)
                methods={"PSOLA_baseline":psola_shift(source,f0,ratio),"phase_vocoder_baseline":phase_vocoder_shift(source,semitones)}
                for method,y in methods.items():
                    rows.append({"vowel":vowel,"f0_hz":f0,"semitones":semitones,"method":method,
                                 "oracle_spectral_envelope_rmse_db":spectral_rmse_db(y,oracle),
                                 "mean_formant_peak_error_hz":formant_peak_error(y,formants)})
    return pd.DataFrame(rows)

def run_envelope_estimators():
    rng=np.random.default_rng(20260907); rows=[]
    for vowel,formants in VOWELS.items():
        for f0 in (110.0,220.0,350.0):
            source=synth_vowel(f0,formants)
            for noise in (0.0,.01):
                x=source[int(.2*FS):int(1.0*FS)].copy()
                if noise: x += noise*rng.standard_normal(len(x))
                harmonics=np.arange(1,int(8000//f0)+1)*f0
                target=20*np.log10(np.maximum(env_curve(harmonics,formants),1e-12)); target-=np.mean(target)
                methods={"LPC12":lambda z:lpc_envelope(z,12),"LPC20":lambda z:lpc_envelope(z,20),
                         "CEP20":lambda z:cepstral_envelope(z,20),"CEP40":lambda z:cepstral_envelope(z,40)}
                for name,fn in methods.items():
                    f,db=fn(x); est=np.interp(harmonics,f,db); est-=np.mean(est)
                    rows.append({"vowel":vowel,"f0_hz":f0,"noise":noise,"method":name,
                                 "log_envelope_rmse_db":float(np.sqrt(np.mean((est-target)**2)))})
    return pd.DataFrame(rows)

def write_demo():
    formants=VOWELS["a"]; f0=180.; semitones=7; ratio=2**(semitones/12)
    source=synth_vowel(f0,formants)
    variants={"demo_source_a_180.wav":source,
              "demo_oracle_formant_preserved_plus7.wav":synth_vowel(f0*ratio,formants),
              "demo_psola_plus7.wav":psola_shift(source,f0,ratio),
              "demo_phase_vocoder_plus7.wav":phase_vocoder_shift(source,semitones)}
    for name,y in variants.items():
        y=y/max(np.max(np.abs(y)),1e-12)*.95
        wavfile.write(OUT/name,FS,(y*32767).astype(np.int16))

def main():
    res=run_resynthesis(); env=run_envelope_estimators()
    res.to_csv(OUT/"resynthesis_v1_results.csv",index=False)
    env.to_csv(OUT/"envelope_estimator_v1_results.csv",index=False)
    write_demo()
    print(res.groupby("method").agg(median_envelope_rmse_db=("oracle_spectral_envelope_rmse_db","median"),mean_formant_peak_error_hz=("mean_formant_peak_error_hz","mean")).round(3))
    print(env.groupby("method").log_envelope_rmse_db.agg(["mean","median","max"]).round(3))
    print("PASS: synthetic research artifacts written")

if __name__=="__main__":
    main()

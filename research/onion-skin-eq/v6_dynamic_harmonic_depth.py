import numpy as np
import pandas as pd

SR = 48000
DUR = 6.0
CTRL_HZ = 200
F0 = 110.0
N = 12
harm = np.arange(1, N + 1)
A0 = np.array([1,.72,.58,.43,.35,.29,.23,.19,.16,.13,.10,.08], float)
B0 = np.array([1,.62,.70,.36,.44,.24,.30,.17,.20,.12,.14,.09], float)
ct = np.arange(int(DUR * CTRL_HZ)) / CTRL_HZ

priority = np.zeros_like(ct)
priority[(ct >= 0) & (ct < 1.5)] = 1.0
priority[(ct >= 1.5) & (ct < 3.0)] = 0.0
priority[(ct >= 3.0) & (ct < 4.5)] = -1.0
priority[(ct >= 4.5)] = 0.0

def smooth_toward(target, attack=.04, release=.18):
    y = np.zeros_like(target, float)
    y[0] = target[0]
    for i in range(1, len(target)):
        tau = attack if abs(target[i]) > abs(y[i-1]) else release
        a = 1 - np.exp(-1 / (CTRL_HZ * tau))
        y[i] = y[i-1] + a * (target[i] - y[i-1])
    return y

An = A0 / A0.max()
Bn = B0 / B0.max()
conf = np.minimum(An, Bn)
max_cut_db = 2.5
A_gain_db = np.zeros((len(ct), N))
B_gain_db = np.zeros((len(ct), N))
move_cents = np.zeros((len(ct), N))

for n in range(2, N):
    Atarget = np.where(priority < 0, -max_cut_db * conf[n], 0.0)
    Btarget = np.where(priority > 0, -max_cut_db * conf[n], 0.0)
    A_gain_db[:, n] = smooth_toward(Atarget)
    B_gain_db[:, n] = smooth_toward(Btarget)
    if n >= 4:
        sign = 1 if n % 2 == 0 else -1
        mt = np.where(priority > 0, sign * 6 * conf[n], np.where(priority < 0, -sign * 6 * conf[n], 0))
        move_cents[:, n] = smooth_toward(mt, attack=.10, release=.25)

def weighted_collision(a, b, cents_b=None):
    vals = []
    for n in range(N):
        distance = 0 if cents_b is None else abs(cents_b[n])
        proximity = np.exp(-(distance / 25.0) ** 2)
        vals.append(min(a[n] / A0.max(), b[n] / B0.max()) * proximity)
    return float(np.mean(vals))

def identity(a, original):
    return float(np.dot(a, original) / (np.linalg.norm(a) * np.linalg.norm(original)))

coll_u, coll_dyn, coll_move, idA, idB = [], [], [], [], []
for k in range(len(ct)):
    a = A0 * 10 ** (A_gain_db[k] / 20)
    b = B0 * 10 ** (B_gain_db[k] / 20)
    coll_u.append(weighted_collision(A0, B0))
    coll_dyn.append(weighted_collision(a, b))
    coll_move.append(weighted_collision(a, b, move_cents[k]))
    idA.append(identity(a, A0))
    idB.append(identity(b, B0))

summary = pd.DataFrame([
    ['Untouched', np.mean(coll_u), 1.0, 1.0, 0.0],
    ['Dynamic harmonic gain', np.mean(coll_dyn), np.mean(idA), np.mean(idB), 0.0],
    ['Gain + bounded movement', np.mean(coll_move), np.mean(idA), np.mean(idB), np.max(np.abs(move_cents))],
], columns=['Strategy','Mean collision','Mean A identity','Mean B identity','Max movement cents'])

for c in summary.columns[1:]:
    summary[c] = summary[c].astype(float).round(4)

assert np.mean(coll_dyn) < np.mean(coll_u)
assert np.mean(coll_move) <= np.mean(coll_dyn)
assert np.max(np.abs(move_cents)) < 6.1
assert min(np.mean(idA), np.mean(idB)) > .99
print(summary.to_csv(index=False))

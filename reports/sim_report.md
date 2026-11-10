fps 25, corner noise 0.70 px, dropout 5%, 20 s per scenario, fx 2400, latency 60 ms, bullet 25 m/s, seed 42

| scenario | direct hit% | point KF hit% | robot EKF hit% | robot hits/s | centre err cm | |w| err rad/s | radius err cm | tracking % |
|---|---|---|---|---|---|---|---|---|
| spin 2.1 rad/s at 5 m (like the videos) | 42.3 | 97.5 | 100.0 | 17.15 | 3.7 | 0.33 | 1.1 | 96 |
| spin 6 rad/s + strafe 0.5 m at 5 m | 53.6 | 14.6 | 49.8 | 6.50 | 3.6 | 0.40 | 0.9 | 96 |
| fast spin 10 rad/s at 4 m | 43.2 | 37.0 | 100.0 | 15.05 | 2.5 | 0.21 | 0.3 | 96 |
| no spin, strafe 0.8 m / 2 s at 6 m | 14.0 | 24.5 | 20.5 | 2.85 | 7.7 | 0.72 | 1.5 | 93 |
| spin 4 rad/s approaching 7 m -> 3 m | 51.8 | 44.4 | 99.3 | 14.10 | 3.4 | 0.27 | 0.6 | 95 |
| variable spin 0.5..4.5 rad/s at 6 m | 69.9 | 73.2 | 91.0 | 12.65 | 4.6 | 0.47 | 0.7 | 96 |
| swing back and forth (+-3 rad/s, 4 s) at 6 m | 64.4 | 88.7 | 98.6 | 17.60 | 4.5 | 0.46 | 0.8 | 97 |
| spin 3 rad/s + gentle strafe 0.3 m / 3 s at 6 m | 74.9 | 67.0 | 65.4 | 9.55 | 4.6 | 0.33 | 0.8 | 95 |

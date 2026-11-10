| strategy | shots | verifiable | hits | hit rate | hits/s |
|---|---|---|---|---|---|
| direct (aim at current plate) | 2848 | 2768 | 1468 | 53.0% | 10.7 |
| point KF (predict plate centre) | 1411 | 1368 | 706 | 51.6% | 5.2 |
| robot EKF (whole-robot model) | 1834 | 1793 | 1154 | 64.4% | 8.4 |
| oracle (cheats: aims at the future plate) | 3183 | 3151 | 2884 | 91.5% | 21.1 |

| prediction | horizon | samples | mean px | median px | p90 px |
|---|---|---|---|---|---|
| no prediction | 0.12 s | 3323 | 52.9 | 51.1 | 76.5 |
| point KF | 0.12 s | 3323 | 80.5 | 65.4 | 129.2 |
| robot EKF | 0.12 s | 3323 | 8.4 | 6.6 | 16.7 |
| no prediction | 0.20 s | 3161 | 36.1 | 22.8 | 94.9 |
| point KF | 0.20 s | 3161 | 88.9 | 45.3 | 202.5 |
| robot EKF | 0.20 s | 3161 | 12.0 | 9.5 | 24.5 |
| no prediction | 0.32 s | 3290 | 45.8 | 43.8 | 81.5 |
| point KF | 0.32 s | 3290 | 147.5 | 65.3 | 260.3 |
| robot EKF | 0.32 s | 3290 | 16.7 | 13.2 | 34.2 |

tracking ratio 0.837, frames with armor 0.979, median |w| 8.348 rad/s, r1 0.214 m, r2 0.237 m, dz -0.046 m

| strategy | shots | verifiable | hits | hit rate | hits/s |
|---|---|---|---|---|---|
| direct (aim at current plate) | 2862 | 2784 | 1734 | 62.3% | 12.7 |
| point KF (predict plate centre) | 1506 | 1455 | 877 | 60.3% | 6.4 |
| robot EKF (whole-robot model) | 2018 | 1978 | 1342 | 67.8% | 9.8 |
| oracle (cheats: aims at the future plate) | 3146 | 3121 | 2959 | 94.8% | 21.7 |

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

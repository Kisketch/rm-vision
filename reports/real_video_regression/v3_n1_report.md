| strategy | shots | verifiable | hits | hit rate | hits/s |
|---|---|---|---|---|---|
| direct (aim at current plate) | 1934 | 1853 | 702 | 37.9% | 7.9 |
| point KF (predict plate centre) | 1751 | 1661 | 446 | 26.9% | 5.0 |
| robot EKF (whole-robot model) | 1398 | 1345 | 764 | 56.8% | 8.6 |
| oracle (cheats: aims at the future plate) | 2001 | 1981 | 1958 | 98.8% | 22.2 |

| prediction | horizon | samples | mean px | median px | p90 px |
|---|---|---|---|---|---|
| no prediction | 0.12 s | 2090 | 35.3 | 26.8 | 86.6 |
| point KF | 0.12 s | 2090 | 40.5 | 9.6 | 118.9 |
| robot EKF | 0.12 s | 2090 | 6.4 | 5.3 | 14.0 |
| no prediction | 0.20 s | 2102 | 42.4 | 43.5 | 76.6 |
| point KF | 0.20 s | 2102 | 61.8 | 28.9 | 126.9 |
| robot EKF | 0.20 s | 2102 | 11.7 | 8.8 | 25.2 |
| no prediction | 0.32 s | 2082 | 40.7 | 37.9 | 77.5 |
| point KF | 0.32 s | 2082 | 101.5 | 59.0 | 146.8 |
| robot EKF | 0.32 s | 2082 | 19.2 | 15.1 | 42.5 |

tracking ratio 0.871, frames with armor 0.965, median |w| 2.678 rad/s, r1 0.217 m, r2 0.221 m, dz 0.042 m

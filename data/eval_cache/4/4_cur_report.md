| strategy | shots | verifiable | hits | hit rate | hits/s |
|---|---|---|---|---|---|
| direct (aim at current plate) | 2927 | 0 | 0 | 0.0% | 0.0 |
| point KF (predict plate centre) | 1138 | 1096 | 472 | 43.1% | 3.5 |
| robot EKF (whole-robot model) | 2039 | 1994 | 1363 | 68.4% | 10.0 |
| sentry (multi-target + outpost) | 554 | 535 | 394 | 73.6% | 2.9 |
| oracle (cheats: aims at the future plate) | 3229 | 3198 | 3001 | 93.8% | 22.0 |

| prediction | horizon | samples | mean px | median px | p90 px |
|---|---|---|---|---|---|
| no prediction | 0.12 s | 3640 | 52.9 | 50.9 | 76.5 |
| point KF | 0.12 s | 3640 | 103.1 | 73.5 | 217.1 |
| robot EKF | 0.12 s | 3640 | 8.4 | 6.8 | 16.4 |
| no prediction | 0.20 s | 3567 | 39.7 | 23.1 | 100.5 |
| point KF | 0.20 s | 3567 | 142.6 | 91.0 | 471.4 |
| robot EKF | 0.20 s | 3567 | 11.9 | 9.5 | 24.2 |
| no prediction | 0.32 s | 3623 | 46.5 | 44.5 | 83.0 |
| point KF | 0.32 s | 3623 | 288.2 | 85.1 | 746.0 |
| robot EKF | 0.32 s | 3623 | 16.6 | 13.4 | 33.6 |

tracking ratio 0.928, frames with armor 0.979, median |w| 8.303 rad/s, r1 0.239 m, r2 0.215 m, dz 0.048 m

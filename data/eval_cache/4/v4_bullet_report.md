| strategy | shots | verifiable | hits | hit rate | hits/s |
|---|---|---|---|---|---|
| direct (aim at current plate) | 2918 | 2833 | 1639 | 57.9% | 12.0 |
| point KF (predict plate centre) | 1079 | 1034 | 443 | 42.8% | 3.2 |
| robot EKF (whole-robot model) | 2042 | 1989 | 1258 | 63.2% | 9.2 |
| oracle (cheats: aims at the future plate) | 3257 | 3222 | 2998 | 93.0% | 21.9 |

| prediction | horizon | samples | mean px | median px | p90 px |
|---|---|---|---|---|---|
| no prediction | 0.12 s | 3652 | 52.9 | 50.8 | 76.5 |
| point KF | 0.12 s | 3652 | 103.0 | 73.6 | 216.3 |
| robot EKF | 0.12 s | 3652 | 8.4 | 6.8 | 16.4 |
| no prediction | 0.20 s | 3579 | 39.7 | 23.1 | 100.4 |
| point KF | 0.20 s | 3579 | 142.4 | 91.0 | 468.6 |
| robot EKF | 0.20 s | 3579 | 11.9 | 9.5 | 24.2 |
| no prediction | 0.32 s | 3636 | 46.4 | 44.4 | 83.0 |
| point KF | 0.32 s | 3636 | 287.6 | 84.9 | 745.7 |
| robot EKF | 0.32 s | 3636 | 16.6 | 13.3 | 33.6 |

tracking ratio 0.932, frames with armor 0.979, median |w| 8.308 rad/s, r1 0.239 m, r2 0.215 m, dz 0.048 m

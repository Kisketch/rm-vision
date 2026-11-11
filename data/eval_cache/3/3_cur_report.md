| strategy | shots | verifiable | hits | hit rate | hits/s |
|---|---|---|---|---|---|
| direct (aim at current plate) | 1943 | 0 | 0 | 0.0% | 0.0 |
| point KF (predict plate centre) | 1743 | 1653 | 489 | 29.6% | 5.5 |
| robot EKF (whole-robot model) | 1441 | 1389 | 938 | 67.5% | 10.6 |
| sentry (multi-target + outpost) | 1098 | 1053 | 729 | 69.2% | 8.3 |
| oracle (cheats: aims at the future plate) | 2010 | 1990 | 1977 | 99.3% | 22.4 |

| prediction | horizon | samples | mean px | median px | p90 px |
|---|---|---|---|---|---|
| no prediction | 0.12 s | 2235 | 36.8 | 27.4 | 88.2 |
| point KF | 0.12 s | 2235 | 47.5 | 9.7 | 121.0 |
| robot EKF | 0.12 s | 2235 | 6.3 | 5.0 | 13.5 |
| no prediction | 0.20 s | 2236 | 43.1 | 44.1 | 77.1 |
| point KF | 0.20 s | 2236 | 80.3 | 29.7 | 130.5 |
| robot EKF | 0.20 s | 2236 | 11.5 | 8.6 | 24.4 |
| no prediction | 0.32 s | 2223 | 41.8 | 39.0 | 79.6 |
| point KF | 0.32 s | 2223 | 459.7 | 66.4 | 154.1 |
| robot EKF | 0.32 s | 2223 | 18.5 | 14.3 | 40.8 |

tracking ratio 0.933, frames with armor 0.965, median |w| 2.690 rad/s, r1 0.216 m, r2 0.232 m, dz -0.050 m

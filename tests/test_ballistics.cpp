// Split from the former single-file test_main.cpp (round 6): same tests,
// grouped by module so a failure points at its subsystem.  Harness: check.hpp.
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <utility>

#include "autoaim/aimer.hpp"
#include "autoaim/ballistics.hpp"
#include "autoaim/config.hpp"
#include "autoaim/delay_calibration.hpp"
#include "autoaim/detector.hpp"
#include "autoaim/evaluation.hpp"
#include "autoaim/keypoint_detector.hpp"
#include "autoaim/pose.hpp"
#include "autoaim/pose_buffer.hpp"
#include "autoaim/tracker.hpp"

#include "check.hpp"

using namespace autoaim;

void testBallistics() {
    BallisticConfig c;
    c.use_drag = false;
    Ballistics ideal(c);
    double h, t;
    CHECK(ideal.simulate(0.0, 5.0, h, t), "ideal simulate");
    const double t_exp = 5.0 / c.bullet_speed;
    CHECK(std::abs(t - t_exp) < 1e-9 && std::abs(h + 0.5 * c.gravity * t_exp * t_exp) < 1e-9,
          "ideal closed form h=%f t=%f", h, t);

    c.use_drag = true;
    Ballistics drag(c);
    double hd, td;
    CHECK(drag.simulate(0.0, 5.0, hd, td), "drag simulate");
    CHECK(td > t && hd < h, "drag must slow the bullet: t %f vs %f, h %f vs %f", td, t, hd, h);
    // RK4 with k = 0 must equal the closed form
    c.drag_k = 0.0;
    double h0, t0;
    Ballistics(c).simulate(0.1, 6.0, h0, t0);
    BallisticConfig ci = c;
    ci.use_drag = false;
    double h1, t1;
    Ballistics(ci).simulate(0.1, 6.0, h1, t1);
    CHECK(std::abs(h0 - h1) < 1e-6 && std::abs(t0 - t1) < 1e-6, "rk4 vs closed form %g %g", h0 - h1, t0 - t1);

    for (double s : {2.0, 5.0, 8.0})
        for (double z : {-0.3, 0.0, 0.4}) {
            const auto sol = drag.solve(s, z);
            CHECK(sol.ok, "solve %f %f", s, z);
            double hh, tt;
            drag.simulate(sol.pitch, s, hh, tt);
            CHECK(std::abs(hh - z) < 1e-3, "solve accuracy s=%f z=%f got %f", s, z, hh);
        }

    // Golden values captured from the implementation before the bullet-speed
    // rework (default 17 mm config).  The tolerance was 1e-9 until the
    // warm-start optimisation of solve() (vacuum-drop initial guess, sqrt in
    // the RK4 derivative): it converges to the same solution within the
    // solver's own |dh| < 5e-4 m criterion, with a measured pitch deviation
    // of at most 6.4e-5 rad (~0.004 deg, vs the 0.2 deg minimum fire
    // tolerance) for a 2.7x speedup.  2e-4 rad still catches real drift.
    {
        Ballistics golden{ BallisticConfig{} };
        const auto s1 = golden.solve(5.0, 0.5);
        CHECK(s1.ok && std::abs(s1.pitch - 0.141715304681) < 2e-4 &&
                  std::abs(s1.t_fly - 0.212001575796) < 1e-5,
              "golden solve(5, 0.5): pitch %.12g t %.12g", s1.pitch, s1.t_fly);
        const auto s2 = golden.solve(8.0, 0.5);
        CHECK(s2.ok && std::abs(s2.pitch - 0.132565507948) < 2e-4 &&
                  std::abs(s2.t_fly - 0.348777463964) < 1e-5,
              "golden solve(8, 0.5): pitch %.12g t %.12g", s2.pitch, s2.t_fly);
    }
    // Measured-speed override: a slower bullet needs more pitch and flies
    // longer; clearing the override restores the config solution exactly.
    {
        Ballistics b{ BallisticConfig{} };
        b.setMeasuredBulletSpeed(23.0);
        CHECK(b.hasMeasuredSpeed() && std::abs(b.bulletSpeed() - 23.0) < 1e-12, "override active");
        const auto slow = b.solve(5.0, 0.5);
        CHECK(slow.ok && slow.pitch > 0.141715304681 && slow.t_fly > 0.212001575796,
              "slower bullet: pitch %f t %f", slow.pitch, slow.t_fly);
        b.setMeasuredBulletSpeed(0.0);
        CHECK(!b.hasMeasuredSpeed(), "override cleared");
        const auto back = b.solve(5.0, 0.5);
        CHECK(std::abs(back.pitch - 0.141715304681) < 2e-4, "golden restored after clear");
    }
    // 42 mm class (hero): its own speed/drag defaults, solvable, and slower
    // than 17 mm over the same shot.
    {
        BallisticConfig c42;
        c42.bullet_mm = 42;
        Ballistics hero(c42);
        CHECK(hero.bulletMm() == 42 && std::abs(hero.bulletSpeed() - c42.bullet_speed_42) < 1e-12 &&
                  std::abs(hero.dragK() - c42.drag_k_42) < 1e-12,
              "42 mm class params");
        const auto s42 = hero.solve(8.0, 0.5);
        const auto s17 = Ballistics{ BallisticConfig{} }.solve(8.0, 0.5);
        CHECK(s42.ok && s17.ok && s42.t_fly > s17.t_fly, "42 mm flies longer: %f vs %f", s42.t_fly,
              s17.t_fly);
        hero.setBulletClass(17);
        CHECK(hero.bulletMm() == 17 && std::abs(hero.bulletSpeed() - 25.0) < 1e-12, "class switch back");
        hero.setBulletClass(20);  // unknown class falls back to 17
        CHECK(hero.bulletMm() == 17, "unknown class -> 17");
    }
}

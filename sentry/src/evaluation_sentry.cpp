// Sentry strategy evaluation, extracted verbatim (logic) from the sentry
// repo's evaluate() in the rm-vision consolidation.  The gimbal starts from
// the same state as every other strategy and the judge is the same
// ShotJudge, so the row is directly comparable with the core strategies.
#include "sentry/evaluation_sentry.hpp"

#include "autoaim/aimer.hpp"
#include "autoaim/ballistics.hpp"
#include "sentry/target_manager.hpp"

namespace autoaim {
namespace {

void fireAndJudge(StrategyResult& sr, const JudgeFn& judge, const GimbalSim& g, const AimCommand& cmd,
                  const std::string& id, int frame, double latency, double stamp, ShotRecord* out) {
    ShotRecord rec;
    rec.frame = frame;
    judge(stamp + latency, g.yaw, g.pitch, cmd.t_fly, id, rec);
    ++sr.shots;
    if (rec.result != ShotResult::UNKNOWN) ++sr.verifiable;
    if (rec.result == ShotResult::HIT) ++sr.hits;
    sr.shots_log.push_back(rec);
    if (out) *out = rec;
}

}  // namespace

void appendSentryStrategy(const Config& cfg, const std::vector<FrameObservations>& obs,
                          const CameraModel& camera, EvalResult& res) {
    const double fps = cfg.eval.fps;
    const double dt = 1.0 / fps;
    Ballistics ball(cfg.ballistic);
    sentry::TargetManager manager(cfg.tracker, cfg.target, camera);
    RobotAimer sentry_aimer(cfg.aimer, ball);
    GimbalSim g_sentry(cfg.aimer.gimbal_tau, cfg.aimer.gimbal_max_rate, cfg.aimer.gimbal_ff);
    ShotJudge video_judge(obs, camera, ball, fps);
    const JudgeFn judge = [&](double t, double yaw, double pitch, double tf, const std::string& id,
                              ShotRecord& rec) {
        rec.result = video_judge.judge(t, yaw, pitch, tf, id, rec);
    };
    const double scale = cfg.aimer.fire_tolerance_scale;

    StrategyResult s_sentry;
    s_sentry.name = "sentry (multi-target + outpost)";
    for (const auto& f : obs) {
        manager.update(f);
        const auto states = manager.snapshots(f.stamp);
        const int si = manager.select(states);
        if (si >= 0) {
            const AimCommand c = sentry_aimer.aim(states[si], f.stamp);
            if (c.valid) {
                g_sentry.step(c.yaw, c.pitch, dt);
                if (c.fire && onTarget(g_sentry, c, states[si].type, scale))
                    fireAndJudge(s_sentry, judge, g_sentry, c, states[si].id, f.frame,
                                 cfg.aimer.latency, f.stamp, nullptr);
            }
        }
    }
    s_sentry.frames = static_cast<int>(obs.size());
    res.strategies.push_back(s_sentry);
}

}  // namespace autoaim

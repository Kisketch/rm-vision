// Sentry strategy for the offline evaluation: TargetManager + TargetState
// aimer over the same observations the core strategies see, appended as an
// extra row of res.strategies.  Split out of evaluation.cpp in the rm-vision
// consolidation — this strategy depends on the sentry module (TargetManager),
// the rest of the evaluation does not.
#pragma once

#include "autoaim/evaluation.hpp"

namespace autoaim {

void appendSentryStrategy(const Config& cfg, const std::vector<FrameObservations>& obs,
                          const CameraModel& camera, EvalResult& res);

}  // namespace autoaim

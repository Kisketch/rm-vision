// Multi-target manager: keeps one tracker per robot identity and chooses the
// aim target explicitly.  The old sentry code kept a single Aim object and
// re-initialised it whenever the first plate under 7 m changed; this keeps the
// history of every target, so switching back does not restart the filter.
//
// This class belongs to the sentry module (namespace sentry, header under
// sentry/), not to the shared core: only the sentry pipeline keeps one track
// per robot identity.  It lived under autoaim/ with namespace autoaim until
// the round-6 engineering review - see docs/engineering_review_r6.md.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "autoaim/config.hpp"
#include "autoaim/tracker.hpp"
#include "autoaim/types.hpp"

namespace sentry {

class TargetManager {
public:
    TargetManager(const autoaim::TrackerConfig& tracker_cfg,
                  const autoaim::TargetConfig& target_cfg,
                  const autoaim::CameraModel& camera);

    // Route the frame's observations to one track per identity (deviceId, or
    // the outpost singleton), update every track (including the ones that
    // were not seen this frame), and drop stale tracks.
    void update(const autoaim::FrameObservations& frame);

    // Snapshots of all active tracks, extrapolated to time t.
    std::vector<autoaim::TargetState> snapshots(double t) const;

    // Index into `snapshots`, or -1.  Uses TargetConfig::priority first, then
    // distance; `require_tracking` rejects DETECTING-only tracks.
    // Stateful (remembers the last selection for switch hysteresis), so it
    // is not const - it used to hide that state behind a mutable member.
    int select(const std::vector<autoaim::TargetState>& states);

    void reset();
    std::size_t size() const { return entries_.size(); }

private:
    struct Entry {
        std::unique_ptr<autoaim::RobotTracker> robot;
        std::unique_ptr<autoaim::OutpostTracker> outpost;
        std::string key;    // deviceId of the seed (or "outpost"): matching identity
        std::string label;  // display label: the plate number (or "outpost")

        double last_seen = 0.0;

        bool active() const;
        autoaim::TrackState state() const;
        autoaim::TargetState snapshot(double t) const;
        void update(const autoaim::FrameObservations& frame);
        void reset();
    };

    int priorityOf(const std::string& label) const;
    bool eligible(const autoaim::TargetState& s) const;

    autoaim::TrackerConfig tracker_cfg_;
    autoaim::TargetConfig target_cfg_;
    const autoaim::CameraModel& camera_;
    std::vector<Entry> entries_;
    std::string last_selected_id_;
};

}  // namespace sentry

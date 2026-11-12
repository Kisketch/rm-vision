// Core test suite entry point.  The tests themselves live in per-module
// files (test_detector.cpp, test_pose.cpp, test_tracker.cpp, test_aimer.cpp,
// test_ballistics.cpp, test_evaluation.cpp, test_timing.cpp,
// test_config.cpp), split from this file in round 6; the harness is
// check.hpp.  Minimal and self-contained: no framework dependency.
#include <cstdio>

#include "check.hpp"

int test_failures = 0;

void testDetectorDegenerateInputs();
void testWindowedDetection();
void testSplitLightMerge();
void testHorizontalBlobRejected();
void testCameraExtrinsic();
void testAimerOffsets();
void testBallistics();
void testPose();
void testPoseQuality();
void testAssociationDiagnostics();
void testTracker();
void testTrackerCaModel();
void testTrackerDropoutGrace();
void testYawProcessNoiseSplit();
void testAimerPhaseLead();
void testShotJudgeNonUniform();
void testDeviceId();
void testAimerRates();
void testKeypointDetector();
void testPoseBuffer();
void testStaleFrameRejected();
void testYawNisOrderRobustness();
void testClockAligner();
void testClockAlignerDrift();
void testPoseBufferExtrapolation();
void testPoseBufferNonFinite();
void testDelayCalibration();
void testConfigTuningRoundTrip();

int main() {
    testDetectorDegenerateInputs();
    testWindowedDetection();
    testSplitLightMerge();
    testHorizontalBlobRejected();
    testCameraExtrinsic();
    testAimerOffsets();
    testBallistics();
    testPose();
    testPoseQuality();
    testAssociationDiagnostics();
    testTracker();
    testTrackerCaModel();
    testTrackerDropoutGrace();
    testYawProcessNoiseSplit();
    testAimerPhaseLead();
    testShotJudgeNonUniform();
    testDeviceId();
    testAimerRates();
    testKeypointDetector();
    testPoseBuffer();
    testStaleFrameRejected();
    testYawNisOrderRobustness();
    testClockAligner();
    testClockAlignerDrift();
    testPoseBufferExtrapolation();
    testPoseBufferNonFinite();
    testDelayCalibration();
    testConfigTuningRoundTrip();
    if (test_failures == 0) std::printf("ALL TESTS PASSED\n");
    return test_failures == 0 ? 0 : 1;
}

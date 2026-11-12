# Shared auto-aim core: the single copy both vehicle pipelines build on.
add_library(autoaim_core STATIC
    src/config.cpp
    src/detector.cpp
    src/pose.cpp
    src/pose_buffer.cpp
    src/delay_calibration.cpp
    src/camera_source.cpp
    src/keypoint_detector.cpp
    src/tracker.cpp
    src/ballistics.cpp
    src/aimer.cpp
    src/evaluation.cpp
    src/draw.cpp)

# Optional Hikvision MVS camera source (G-02 step 1). Off by default so the
# offline pipeline builds anywhere; enable on the robot with the SDK path:
#   cmake -DAUTOAIM_WITH_MVS=ON -DMVS_ROOT=/opt/MVS ..
option(AUTOAIM_WITH_MVS "Build the Hikvision MVS camera source" OFF)
if (AUTOAIM_WITH_MVS)
    set(MVS_ROOT "/opt/MVS" CACHE PATH "Hikvision MVS SDK root")
    find_path(MVS_INCLUDE_DIR MvCameraControl.h HINTS ${MVS_ROOT}/include)
    find_library(MVS_LIBRARY MvCameraControl HINTS ${MVS_ROOT}/lib/64 ${MVS_ROOT}/lib)
    if (NOT MVS_INCLUDE_DIR OR NOT MVS_LIBRARY)
        message(FATAL_ERROR "AUTOAIM_WITH_MVS=ON but the MVS SDK was not found under ${MVS_ROOT}")
    endif()
    # The SDK headers/library are only used inside camera_source.cpp, so
    # they stay PRIVATE; the AUTOAIM_WITH_MVS define stays PUBLIC because
    # camera_source.hpp and sentry_robot.cpp both #ifdef on it.
    target_include_directories(autoaim_core PRIVATE ${MVS_INCLUDE_DIR})
    target_link_libraries(autoaim_core PRIVATE ${MVS_LIBRARY})
    target_compile_definitions(autoaim_core PUBLIC AUTOAIM_WITH_MVS)
endif()
target_include_directories(autoaim_core PUBLIC include ${OpenCV_INCLUDE_DIRS})
target_link_libraries(autoaim_core PUBLIC ${OpenCV_LIBS})
add_library(autoaim::core ALIAS autoaim_core)
# AUTOAIM_ROOT points at core/ so the historical "/config/default.yaml" and
# "/model/mlp.onnx" suffixes keep resolving after the consolidation.
# AUTOAIM_ROOT stays PUBLIC on purpose: the apps and tests (not just this
# library) resolve the default config / model paths through it - see
# apps/autoaim_video.cpp and tests/test_sentry.cpp.  It points at core/ so
# the historical "/config/default.yaml" and "/model/mlp.onnx" suffixes keep
# resolving after the consolidation.  _USE_MATH_DEFINES /
# _CRT_SECURE_NO_WARNINGS are likewise consumer-facing MSVC compatibility
# defines.  Warning/encoding OPTIONS, by contrast, are this library's own
# business and are PRIVATE since round 6.
target_compile_definitions(autoaim_core PUBLIC AUTOAIM_ROOT="${CMAKE_CURRENT_SOURCE_DIR}" _USE_MATH_DEFINES
                           _CRT_SECURE_NO_WARNINGS)
if (MSVC)
    target_compile_options(autoaim_core PRIVATE /utf-8 /W3)
else()
    target_compile_options(autoaim_core PRIVATE -Wall -Wextra)
endif()

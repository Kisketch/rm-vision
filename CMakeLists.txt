cmake_minimum_required(VERSION 3.16)
project(rm_vision LANGUAGES CXX)

# rm-vision: the consolidated RoboMaster vision auto-aim repository
# (rm-autoaim + rm-sentry-integration merged 2026-11-10; see README.md).
#   core/    shared auto-aim core (one copy — detection, PnP, EKF, ballistics)
#   sentry/  sentry-only module (multi-target manager, serial, ROI, outpost)
#   apps/    infantry/hero offline apps; sentry apps live in sentry/apps/
#   tests/   unit tests for both modules

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(BUILD_SENTRY "Build the sentry module and its apps" ON)
option(BUILD_APPS "Build the offline applications" ON)

find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs videoio highgui calib3d dnn)

# BUILD_TESTING comes from CTest (default ON): -DBUILD_TESTING=OFF skips
# the test targets entirely.
include(CTest)
add_subdirectory(core)
if (BUILD_APPS)
    add_subdirectory(apps)
endif()
if (BUILD_SENTRY)
    add_subdirectory(sentry)
endif()
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

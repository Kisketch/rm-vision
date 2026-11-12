# Sentry module: multi-target management, serial link, tracking-guided ROI,
# outpost tracking and the sentry strategy evaluation.  Builds on autoaim_core.
add_library(autoaim_sentry STATIC
    src/target.cpp
    src/sentry.cpp
    src/roi.cpp
    src/serial_protocol.cpp
    src/evaluation_sentry.cpp)
if (UNIX AND NOT APPLE)
    target_sources(autoaim_sentry PRIVATE src/io/serial_port.cpp)
endif()
target_include_directories(autoaim_sentry PUBLIC include)
target_link_libraries(autoaim_sentry PUBLIC autoaim_core)
add_library(autoaim::sentry ALIAS autoaim_sentry)
target_compile_definitions(autoaim_sentry PUBLIC AUTOAIM_WITH_SENTRY
                           SENTRY_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(sentry_offline apps/sentry_offline.cpp)
target_link_libraries(sentry_offline PRIVATE autoaim_sentry)

if (UNIX AND NOT APPLE)
    add_executable(sentry_robot apps/sentry_robot.cpp)
    target_link_libraries(sentry_robot PRIVATE autoaim_sentry)
endif()

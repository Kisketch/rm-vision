# Infantry/hero offline apps, built on the shared core.
add_executable(autoaim_video autoaim_video.cpp)
target_link_libraries(autoaim_video PRIVATE autoaim_core)
if (BUILD_SENTRY)
    # Gives autoaim_video the sentry strategy row (AUTOAIM_WITH_SENTRY).
    target_link_libraries(autoaim_video PRIVATE autoaim_sentry)
endif()

add_executable(autoaim_sim autoaim_sim.cpp)
target_link_libraries(autoaim_sim PRIVATE autoaim_core)

add_executable(autoaim_lights autoaim_lights.cpp)
target_link_libraries(autoaim_lights PRIVATE autoaim_core)

add_executable(detector_bench detector_bench.cpp)
target_link_libraries(detector_bench PRIVATE autoaim_core)

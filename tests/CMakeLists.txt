add_executable(autoaim_tests test_main.cpp test_detector.cpp test_pose.cpp
               test_tracker.cpp test_aimer.cpp test_ballistics.cpp
               test_evaluation.cpp test_timing.cpp test_config.cpp)
target_link_libraries(autoaim_tests PRIVATE autoaim_core)
add_test(NAME autoaim_tests COMMAND autoaim_tests)

# The regression gate's PASS/FAIL contract tests (pure Python, no build
# inputs beyond the tools/ sources) - previously only run by hand.
find_program(RM_PYTHON3 NAMES python3)
if (RM_PYTHON3)
    add_test(NAME regression_gate_contract
             COMMAND ${RM_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/test_regression_gate.py)
endif()

# Every public header must compile on its own (no reliance on transitive
# includes) - see tools/check_headers.sh.
if (UNIX)
    add_test(NAME header_self_containment
             COMMAND bash ${CMAKE_SOURCE_DIR}/tools/check_headers.sh
                     ${CMAKE_SOURCE_DIR}/core/include ${CMAKE_SOURCE_DIR}/sentry/include
                     ${OpenCV_INCLUDE_DIRS})
endif()

if (BUILD_SENTRY)
    add_executable(sentry_tests test_sentry.cpp)
    target_link_libraries(sentry_tests PRIVATE autoaim_sentry)
    add_test(NAME sentry_tests COMMAND sentry_tests)

    if (UNIX AND NOT APPLE)
        add_executable(serial_port_tests serial_port_test.cpp)
        target_link_libraries(serial_port_tests PRIVATE autoaim_sentry util)
        add_test(NAME serial_port_tests COMMAND serial_port_tests)
    endif()
endif()

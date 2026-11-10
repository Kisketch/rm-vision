cmake_minimum_required(VERSION 3.0)

project(VISION)

# 设置构建类型为 Release
set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在设置全局标志之前，先设置Release模式的优化级别
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall")
set(CMAKE_C_FLAGS_RELEASE "-O2")

# 全局编译标志（会被特定构建类型的标志覆盖）
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# 查找必要的包
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Ceres REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

# 设置包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/Backbone/include
    ${CMAKE_SOURCE_DIR}/io/include
    ${CMAKE_SOURCE_DIR}/Armor/include
    ${CMAKE_SOURCE_DIR}/Buff/include
    ${CMAKE_SOURCE_DIR}/Test/include
    ${CMAKE_SOURCE_DIR}/tools
    ${OpenCV_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIR}
    ${CERES_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIRS}
    "/opt/MVS/include"
    ${LIBUSB_INCLUDE_DIRS}
)

# 设置库文件目录
link_directories("/opt/MVS/lib")

# 查找源文件
file(GLOB_RECURSE sourceFiles
    "${CMAKE_SOURCE_DIR}/Backbone/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/io/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/Armor/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/Buff/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/Test/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/tools/*.cpp"
)
list(FILTER sourceFiles EXCLUDE REGEX ".*/build/.*")

# 添加可执行目标
add_executable(action ${sourceFiles})

# 查找 MVS 库
find_library(MVS libMvCameraControl.so PATHS /opt/MVS/lib /opt/MVS/lib/64)

# 链接库 - 修正libusb链接
target_link_libraries(action
    ${OpenCV_LIBS}
    fmt::fmt
    Threads::Threads
    ${EIGEN3_LIBRARIES}
    ${CERES_LIBRARIES}
    ${MVS}
    ${YAML_CPP_LIBRARIES}
    ${LIBUSB_LIBRARIES}
)

# 输出包含目录的路径
message(STATUS "OpenCV include directories: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "Eigen include directories: ${EIGEN3_INCLUDE_DIR}")
message(STATUS "Ceres include directories: ${CERES_INCLUDE_DIRS}")
message(STATUS "yaml-cpp include directories: ${YAML_CPP_INCLUDE_DIRS}")
message(STATUS "libusb libraries: ${LIBUSB_LIBRARIES}")
message(STATUS "libusb include dirs: ${LIBUSB_INCLUDE_DIRS}")

# 验证编译标志
message(STATUS "CXX Flags for Release: ${CMAKE_CXX_FLAGS_RELEASE}")
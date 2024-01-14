# cmake最低版本
cmake_minimum_required(VERSION 3.14)
# 本项目名
set(PROJECT_NAME sdk_common_demo)
project(${PROJECT_NAME} VERSION "1.2")

# 设置C++17标准，需要满足标准，不用C++拓展语法
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-std=c++17 -pthread -g -Wall -O0 -fstack-protector -fstack-protector-all  -Wno-error=register)

set(QT_PATH /opt/Qt/5.15.2/gcc_64)
set(CMAKE_PREFIX_PATH ${QT_PATH})
message("-- The qt path:${CMAKE_PREFIX_PATH}")

set(Qt5_DIR "${CMAKE_PREFIX_PATH}/lib/cmake/Qt5")
message("The qt5 dir:${Qt5_DIR}")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
# set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)


# 进入logger文件夹下面的CMakeLists.txt
add_subdirectory(logger)

# 进入sdk_net文件夹下面的CMakeLists.txt
add_subdirectory(sdk_net)

# 进入demo文件夹下面的CMakeLists.txt
add_subdirectory(demo)



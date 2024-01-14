cmake_minimum_required(VERSION 3.14)

project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_definitions(-O2 -pthread)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../logger)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../sdk_net)

set(demo_src
    ../logger/rslogger_declare.h
    ../logger/rslogging.h
    ../logger/rslog.h
    ../sdk_net/publicparam.h
    ../sdk_net/netclient.h
    main.cpp
)

add_executable(sdk_common_demo ${demo_src})

target_link_libraries(${PROJECT_NAME}  Qt${QT_VERSION_MAJOR}::Core logger sdk_net)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

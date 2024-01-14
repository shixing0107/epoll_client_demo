cmake_minimum_required(VERSION 3.14)

project(sdk_net LANGUAGES CXX)
set(PROJECT_NAME sdk_net)
project(${PROJECT_NAME} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_definitions(-O2 -pthread)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../logger)

set(demo_src
    ../logger/rslogger_declare.h
    ../logger/rslogging.h
    ../logger/rslog.h
    common_declare.h
    publicparam.h
    utils.h
    utils.cpp
    rpevents.h
    rpevents.cpp
    netclient.h
    netclient.cpp
    bufferpool.h
    bufferpool.cpp
    epollclient.h
    epollclient.cpp
    socketpacket.h
    socketpacket.cpp
    ringqueuepacket.h
    ringqueuepacket.cpp
    tcpipsercnt.h
    tcpipsercnt.cpp
)

add_library(${PROJECT_NAME} SHARED ${demo_src})

target_link_libraries(${PROJECT_NAME}  logger)


cmake_minimum_required(VERSION 3.3)
project(sempr_lang)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(COMMON_SOURCE_FILES
    atomic.hpp
    cerl.hpp
    channel.cpp
    channel.hpp
    common.hpp
    condition.hpp
    constant.hpp
    lock.hpp
    log.cpp
    log.hpp
    port.cpp
    port.hpp
    port_service.cpp
    port_service.hpp
    tasklet.cpp
    tasklet.hpp
    tasklet_service.cpp
    tasklet_service.hpp
    thread.hpp
    threadlet.cpp
    threadlet.hpp
    throwable.hpp
    timer.cpp
    timer.hpp
)

set(TEST_HELLOWORLD_SOURCE_FILES
    ${COMMON_SOURCE_FILES}
    test_helloworld.cpp
)

set(TEST_PORT_SERVICE_SOURCE_FILES
    ${COMMON_SOURCE_FILES}
    test_port_service.cpp
)

set(TEST_PREFORK_SOURCE_FILES
    ${COMMON_SOURCE_FILES}
    test_prefork.cpp
)

set(TEST_TASKLET_SOURCE_FILES
    ${COMMON_SOURCE_FILES}
    test_tasklet.cpp
)


add_executable(test_helloworld ${TEST_HELLOWORLD_SOURCE_FILES})
add_executable(test_port_service ${TEST_PORT_SERVICE_SOURCE_FILES})
add_executable(test_prefork ${TEST_PREFORK_SOURCE_FILES})
add_executable(test_tasklet ${TEST_TASKLET_SOURCE_FILES})

target_link_libraries(test_helloworld pthread log4cplus)
target_link_libraries(test_port_service pthread log4cplus)
target_link_libraries(test_prefork pthread log4cplus)
target_link_libraries(test_tasklet pthread log4cplus)

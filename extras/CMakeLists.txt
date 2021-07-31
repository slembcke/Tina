cmake_minimum_required(VERSION 3.8)

project(tina-extras)
set(CMAKE_C_STANDARD 11)

include_directories(. ..)

set(COMMON common/common.c common/libs/tinycthread.c )

add_executable(test-jobs-throughput test/jobs-throughput.c ${COMMON})
add_executable(test-jobs-wait test/jobs-wait.c ${COMMON})

add_executable(examples-coro-simple examples/coro-simple.c ${COMMON})
add_executable(examples-coro-symmetric examples/coro-symmetric.c ${COMMON})

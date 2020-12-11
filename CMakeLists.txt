cmake_minimum_required(VERSION 3.1)
message( STATUS "cmake: ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}" )

project (hp_server)
set (CMAKE_BUILD_TYPE Debug)

# c++ 11/14/17
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

# output path: build/
# PROJECT_BINARY_DIR       --- cmake 编译所在的目录 path
# CMAKE_CURRENT_SOURCE_DIR --- current CMakeLists.txt path
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

message(STATUS "[__INFO__] CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message(STATUS "[__INFO__] PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")

# config
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h"
)
# pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads)

aux_source_directory(applica APPLICA)
aux_source_directory(timer TIMER)
aux_source_directory(epoll EPOLL)
aux_source_directory(log LOG)
include_directories(applica timer epoll)
#add_library(applica_lib ${APPLICA})
add_library(epoll_lib ${EPOLL})
add_library(timer_lib ${TIMER})
add_library(log_lib ${LOG})
add_library(hp_lib web_server.cpp threadpool.h lock.h)
add_executable(hp_server main.cpp)

target_link_libraries(hp_server hp_lib timer_lib epoll_lib log_lib ${CMAKE_THREAD_LIBS_INIT}) 
# target link 的顺序很重要，如果hp_lib中需要依赖timer_lib，就应该把hp_lib放在timer_lib的前面，链接是一个有向图的过程
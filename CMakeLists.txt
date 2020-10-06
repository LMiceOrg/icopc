cmake_minimum_required(VERSION 3.1)

project(icOPC)

# C/C++ settings
IF( ${CMAKE_CXX_COMPILER} MATCHES  "[cg][+][+]")
    MESSAGE("G++ ")
    # -Wno-unused-parameter -Wno-unused-local-typedef -fheinous-gnu-extensions
    set(COMMON_FLAGS " -Wno-unused-parameter")
    set(OpenMP_FLAGS, " -fopenmp -fopenmp-simd -ftree-vectorizer-verbose=1 -ftree-vectorize")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -std=c++11")
    set(CMAKE_CXX_FLAGS_DEBUG "-g3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    # -malign-double
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -march=native -mtune=native")

    set(CMAKE_C_FLAGS "-Wall -Wextra -Wpedantic -std=gnu99")
    set(CMAKE_C_FLAGS_DEBUG "-g3")
    set(CMAKE_C_FLAGS_RELEASE "-O3")
    # -malign-double
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS} -march=native -mtune=native")

    #add_definitions(-D_BSD_SOURCE)
    #add_definitions(-D__USE_POSIX199309)
    #add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++11> $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
ELSE()

ENDIF()

FIND_LIBRARY(jansson_libs NAMES jansson
                PATHS /usr/local /usr
            )
FIND_PATH(jansson_header jansson.h)
INCLUDE_DIRECTORIES(${jansson_header})

# WTL
INCLUDE_DIRECTORIES(WTL10_1077/include)

# CMake settings
set(CMAKE_VERBOSE_MAKEFILE on)
set(EXECUTABLE_OUTPUT_PATH  "${CMAKE_BINARY_DIR}")
set(LIBRARY_OUTPUT_PATH  "${CMAKE_BINARY_DIR}")

# websocket lib
FIND_PATH(websocket_header wsserver.h
    PATHS websocket)
LIST(APPEND websocket_SRCS

    websocket/base64.c
    websocket/base64.h
    websocket/sha1.c
    websocket/sha1.h
    websocket/wsserver.c
    websocket/wsserver.h
    websocket/iocpserver.c
    websocket/iocpserver.h

    libs/sms/sm4.h
    libs/sms/sm4.c
    )

#Static websocket Library
ADD_LIBRARY(websocket_static STATIC ${websocket_SRCS})

SET_TARGET_PROPERTIES(websocket_static PROPERTIES OUTPUT_NAME "websocket")
SET_TARGET_PROPERTIES(websocket_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# test websocket
add_executable( test_websocket  websocket/main.c)
target_link_libraries( test_websocket  ${jansson_libs} websocket_static)


# win server
LIST(APPEND winserver_SRCS

    winserver/config.cpp
    winserver/config.h
    winserver/icopc_rpc.cpp
    winserver/icopc_rpc.h
    winserver/icopc_server.cpp
    winserver/icopc_server.h
    winserver/stdafx.cpp
    winserver/stdafx.h
    winserver/icopc_service.cpp
    winserver/win_server.cpp
    winserver/win_server.h
    )

add_executable( winserver ${winserver_SRCS})
target_link_libraries( winserver  ${jansson_libs} websocket_static)

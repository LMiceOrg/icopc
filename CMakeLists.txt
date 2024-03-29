cmake_minimum_required(VERSION 3.18)
set(HAVE_FLAG_SEARCH_PATHS_FIRST 0)
SET(CMAKE_SYSTEM_NAME Windows)

project(icOPC)
set(CMAKE_C_LINK_FLAGS "")
set(CMAKE_CXX_LINK_FLAGS "")

# C/C++ settings
message("${CMAKE_CXX_COMPILER}")
IF( ${CMAKE_CXX_COMPILER} MATCHES  "[cg][+][+]")
    MESSAGE("G++  ${CMAKE_CXX_COMPILER}")
    IF(${CMAKE_CXX_COMPILER} MATCHES "i686")
        message(" link: 686")
        #SET(CMAKE_EXE_LINKER_FLAGS "")
        set(COMMON_FLAGS "-m32 -march=i686 ")

    ELSE()
    ENDIF()
    # -Wno-unused-parameter -Wno-unused-local-typedef -fheinous-gnu-extensions
    set(COMMON_FLAGS " ${COMMON_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-parameter -msse2 -mtune=haswell ")
    set(OpenMP_FLAGS, " -fopenmp -fopenmp-simd -ftree-vectorizer-verbose=1 -ftree-vectorize")

    set(CMAKE_CXX_FLAGS "-std=c++11 -fno-rtti ")
    set(CMAKE_CXX_FLAGS_DEBUG "-g3 -D_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Os -s")
    # -malign-double  -march=native -mtune=native
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}  ")

    #-std=gnu99
    set(CMAKE_C_FLAGS "-std=c90 -Dinline=__inline")
    #set(CMAKE_C_FLAGS "-std=c99 -Dinline=__inline")
    set(CMAKE_C_FLAGS_DEBUG "-g3 -D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -Os -s")
    # -malign-double
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}  ")


    message("c  flag:${CMAKE_C_FLAGS} ")
    message("c++flag:${CMAKE_CXX_FLAGS} ")

ELSE()

ENDIF()

# host system name
if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    if(MINGW)
        message("host: ${CMAKE_HOST_SYSTEM_NAME}-mingw target: ${CMAKE_SYSTEM_NAME}")
        add_definitions("-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")
    endif()
elseif (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Windows")
    if(MINGW)
        message("host: ${CMAKE_HOST_SYSTEM_NAME}-mingw target: ${CMAKE_SYSTEM_NAME}")
        add_definitions("-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")
    endif()
endif()

# FIND_LIBRARY(jansson_libs NAMES jansson
#                 PATHS /usr/local /usr
#             )
# FIND_PATH(jansson_header jansson.h)
# INCLUDE_DIRECTORIES(${jansson_header})

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
target_include_directories(test_websocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libs/jansson/src)
target_link_libraries( test_websocket  websocket_static jansson_static ws2_32 atomic m pthread)


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
target_link_libraries( winserver  jansson_static websocket_static)

# Doc
LIST(APPEND doc_SRCS
    docs/model-arch.qmodel
    )
add_custom_target(doc ALL SOURCES  ${doc_SRCS})


# jansson
LIST(APPEND jansson_SRCS
libs/jansson/src/dump.c
libs/jansson/src/error.c
libs/jansson/src/hashtable_seed.c
libs/jansson/src/hashtable.c
libs/jansson/src/load.c
libs/jansson/src/memory.c
libs/jansson/src/pack_unpack.c
libs/jansson/src/strbuffer.c
libs/jansson/src/strconv.c
libs/jansson/src/utf.c
libs/jansson/src/value.c
libs/jansson/src/version.c
)

#Static jansson Library
ADD_LIBRARY(jansson_static STATIC ${jansson_SRCS})
target_compile_definitions(jansson_static PUBLIC HAVE_CONFIG_H)
target_include_directories(jansson_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libs/jansson/src)

SET_TARGET_PROPERTIES(jansson_static PROPERTIES OUTPUT_NAME "jansson")
SET_TARGET_PROPERTIES(jansson_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)


# OPCDA2 static library
LIST(APPEND opcda2_SRCS
opcda2/opcda2_all.c
opcda2/server.c
opcda2/callback.c
opcda2/group.c
#opcda2/util_list.c
opcda2/util_hash.c
opcda2/util_trace.c
opcda2/util_ticketlock.c
)
ADD_LIBRARY(opcda2_static STATIC ${opcda2_SRCS})
target_compile_definitions(opcda2_static PUBLIC "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")

SET_TARGET_PROPERTIES(opcda2_static PROPERTIES OUTPUT_NAME "opcda2")
SET_TARGET_PROPERTIES(opcda2_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# opcda2 测试程序
LIST(APPEND testopc_SRCS
opcda2/test_opc.c
)

add_executable(testopc ${testopc_SRCS})
target_link_libraries( testopc  opcda2_static -lole32 -loleaut32)
target_compile_definitions(testopc PUBLIC "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")

# asyncio library
LIST(APPEND asyncio_SRCS
lmiced/iocpserver.c
lmiced/heap.c
lmiced/publish.c
)
ADD_LIBRARY(asyncio_static STATIC ${asyncio_SRCS})
#target_compile_definitions(asyncio_static PUBLIC "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")
SET_TARGET_PROPERTIES(asyncio_static PROPERTIES OUTPUT_NAME "asyncio")
SET_TARGET_PROPERTIES(asyncio_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)


#EAL static library
LIST(APPEND eal_SRCS
lmiced/eal/lmice_eal_hash.c
)
ADD_LIBRARY(eal_static STATIC ${eal_SRCS})
SET_TARGET_PROPERTIES(eal_static PROPERTIES OUTPUT_NAME "eal")
SET_TARGET_PROPERTIES(eal_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

#lmiced windows service
LIST(APPEND lmiced_SRCS
lmiced/winmain.c
)

add_executable(lmiced ${lmiced_SRCS})
target_link_libraries( lmiced  asyncio_static ws2_32 eal_static)
target_compile_definitions(lmiced PUBLIC "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")


#lmspi
LIST(APPEND lmspi_SRCS
lmiced/spi.c
)
ADD_LIBRARY(lmspi_static STATIC ${lmspi_SRCS})
#target_compile_definitions(lmspi_static PUBLIC "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")
SET_TARGET_PROPERTIES(lmspi_static PROPERTIES OUTPUT_NAME "lmspi")
SET_TARGET_PROPERTIES(lmspi_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

#testspi
LIST(APPEND testspi_SRCS
lmiced/testspi.c
)
add_executable(testspi ${testspi_SRCS})
target_link_libraries( testspi  eal_static)
target_compile_definitions(testspi PUBLIC "-DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0401")

cmake_minimum_required(VERSION 2.8)
project(adapt)

#Use -DCMAKE_BUILD_TYPE=<Release|Debug>
#
# -DCFG_DIR=<libconfig install path>
# alternatively:
# -DCFG_INC=<libconfig include path>
# -DCFG_LIB=<libconfig library path>
#
# -DCPU_DIR=<libcpufreq install path>
# alternatively:
# -DCPU_INC=<libcpufreq include path>
# -DCPU_LIB=<libcpufreq library path>
#
# Disable cpu frequency changing
# -DNO_CPUFREQ=On
# Disable x86 adapt kernel module interaction
# -DNO_X86_ADAPT=On


# You may give this option ...
option(CFG_DIR "Path to include/libconfig.h and lib/libconfig.so")
# ... or these
option(CFG_INC "Path to libconfig.h")
option(CFG_LIB "Path to libconfig.so")

# You may give this option ...
option(CPU_DIR "Path to include/cpufreq.h and lib/libcpufreq.so")
# ... or these
option(CPU_INC "Path to libcpufreq.h")
option(CPU_LIB "Path to libcpufreq.so")

# You may give this option ...
option(XA_DIR "Path to include/x86_adapt.h and lib/libx86_adapt.so")
# ... or these
option(XA_INC "Path to x86_adapt.h")
option(XA_LIB "Path to libx86_adapt.so")

# Disable x86_adapt which is not so common
option(NO_X86_ADAPT "Disable x86_adapt kernel module support")

# Disable cpu frequency changing
option(NO_CPUFREQ "Disable cpu frequency changing")

#debug c flags
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -std=c99")

#release c flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -std=c99")

#shared c flags among build types
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99 -D_POSIX_C_SOURCE -D_BSD_SOURCE -D_GNU_SOURCE")

#add all .c files
unset(SOURCES CACHE)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SOURCES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/knobs SOURCES)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


if(${NO_X86_ADAPT})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_X86_ADAPT")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/knobs/x86_adapt_items.c")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/knobs/x86_adapt_items.h")
endif(${NO_X86_ADAPT})


if(${NO_CPUFREQ})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNO_CPUFREQ")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/knobs/dvfs.c")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/knobs/dvfs.h")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/knobs/fastcpufreq.c")
    list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/knobs/fastcpufreq.h")
	message(STATUS "cpufreq disabled")
endif(${NO_CPUFREQ})

unset(INCTMP CACHE)
find_path(INCTMP execinfo.h)
if(NOT IS_ABSOLUTE ${INCTMP})
  message(FATAL_ERROR "Could not find execinfo.h.")
endif(NOT IS_ABSOLUTE ${INCTMP})
message(STATUS "Found execinfo.h in ${INCTMP}")

unset(INCTMP CACHE)
find_path(INCTMP libconfig.h HINTS ${CFG_INC} ${CFG_DIR}/include)
if(NOT IS_ABSOLUTE ${INCTMP})
  message(FATAL_ERROR "Could not find libconfig.h.")
endif(NOT IS_ABSOLUTE ${INCTMP})
message(STATUS "Found libconfig.h in ${INCTMP}")
include_directories(${INCTMP})

unset(INCTMP CACHE)
find_path(INCTMP dlfcn.h)
if(NOT IS_ABSOLUTE ${INCTMP})
  message(FATAL_ERROR "Could not find dlfcn.h.")
endif(NOT IS_ABSOLUTE ${INCTMP})
message(STATUS "Found dlfcn.h in ${INCTMP}")

unset(INCTMP CACHE)
if(NOT ${NO_CPUFREQ})
find_path(INCTMP cpufreq.h HINTS ${CPU_INC} ${CPU_DIR}/include)
if(NOT IS_ABSOLUTE ${INCTMP})
  message(FATAL_ERROR "Could not find cpufreq.h")
endif(NOT IS_ABSOLUTE ${INCTMP})
message(STATUS "Found cpufreq.h in ${INCTMP}")
include_directories(${INCTMP})
endif(NOT ${NO_CPUFREQ})

unset(INCTMP CACHE)
if(NOT ${NO_X86_ADAPT})
find_path(INCTMP x86_adapt.h HINTS ${XA_INC} ${XA_DIR}/include)
if(NOT IS_ABSOLUTE ${INCTMP})
  message(FATAL_ERROR "Could not find x86_adapt.h")
endif(NOT IS_ABSOLUTE ${INCTMP})
message(STATUS "Found x86_adapt.h in ${INCTMP}")
include_directories(${INCTMP})
endif(NOT ${NO_X86_ADAPT})

unset(INCTMP CACHE)
find_path(INCTMP regex.h)
if(NOT IS_ABSOLUTE ${INCTMP})
  message(FATAL_ERROR "Could not find regex.h")
endif(NOT IS_ABSOLUTE ${INCTMP})
message(STATUS "Found regex.h in ${INCTMP}")


unset(LIBTMP CACHE)
if(NOT ${NO_CPUFREQ})
find_library(LIBTMP libcpufreq.so HINTS ${CPU_LIB} ${CPU_DIR}/lib)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libcpufreq.so.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libcpufreq.so in ${LIBTMP}.")
set(LIBCPU "${LIBTMP}")
endif(NOT ${NO_CPUFREQ})

unset(LIBTMP CACHE)
if(NOT ${NO_X86_ADAPT})
find_library(LIBTMP libx86_adapt.so HINTS ${XA_LIB} ${XA_DIR}/lib)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libx86_adapt.so.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libx86_adapt.so in ${LIBTMP}.")
set(LIBXA "${LIBTMP}")
endif(NOT ${NO_X86_ADAPT})

unset(LIBTMP CACHE)
find_library(LIBTMP libdl.so)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libdl.so.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libdl.so in ${LIBTMP}.")
set(LIBDL "${LIBTMP}")

unset(LIBTMP CACHE)
find_library(LIBTMP libconfig.so HINTS ${CFG_LIB} ${CFG_DIR}/lib)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libconfig.so.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libconfig.so in ${LIBTMP}.")
set(LIBCFG "${LIBTMP}")

unset(LIBTMP CACHE)
if(NOT ${NO_CPUFREQ})
find_library(LIBTMP libcpufreq.a HINTS ${CPU_LIB} ${CPU_DIR}/lib)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libcpufreq.a.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libcpufreq.a in ${LIBTMP}.")
set(LIBCPUA "${LIBTMP}")
endif(NOT ${NO_CPUFREQ})

unset(LIBTMP CACHE)
if(NOT ${NO_X86_ADAPT})
find_library(LIBTMP libx86_adapt_static.a HINTS ${XA_LIB} ${XA_DIR}/lib)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libx86_adapt_static.a.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libx86_adapt_static.a in ${LIBTMP}.")
set(LIBXAA "${LIBTMP}")
endif(NOT ${NO_X86_ADAPT})

unset(LIBTMP CACHE)
find_library(LIBTMP libdl.a)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libdl.a.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libdl.a in ${LIBTMP}.")
set(LIBDLA "${LIBTMP}")

unset(LIBTMP CACHE)
find_library(LIBTMP libconfig.a HINTS ${CFG_LIB} ${CFG_DIR}/lib)
if(NOT IS_ABSOLUTE ${LIBTMP})
  message(FATAL_ERROR "Could not find libconfig.a.")
endif(NOT IS_ABSOLUTE ${LIBTMP})
message(STATUS "Found libconfig.a in ${LIBTMP}.")
set(LIBCFGA "${LIBTMP}")

#build shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(${PROJECT_NAME}_dummy STATIC ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBCPU} ${LIBDL} ${LIBCFG} ${LIBXA})

# now some magic to merge static librarys
set(TARGET ${CMAKE_BINARY_DIR}/libadapt_static.a)
set(STATIC_LIBS ${CMAKE_BINARY_DIR}/libadapt_dummy.a ${LIBCPUA} ${LIBDLA} ${LIBCFGA} ${LIBXAA})

add_custom_target(libadapt_static.a ALL
                COMMAND ./merge_static_libs.sh ${TARGET} ${STATIC_LIBS}
                DEPENDS ${PROJECT_NAME}_dummy
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

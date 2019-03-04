
#
# CMake defines to cross-compile to ARM/Linux on BCM2708 using glibc.
#

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
SET(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
SET(CMAKE_SYSTEM_PROCESSOR arm)

#ADD_DEFINITIONS("-march=armv6")
#add_definitions("-mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -marm")

# rdynamic means the backtrace should work
IF (CMAKE_BUILD_TYPE MATCHES "Debug")
   add_definitions(-rdynamic)
ENDIF()

# avoids annoying and pointless warnings from gcc
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE")
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -c")

cmake_minimum_required(VERSION 2.8)

SET(COMPILE_DEFINITIONS -Werror -Wall)
include_directories(${PROJECT_SOURCE_DIR}/host_applications/linux/libs/bcm_host/include)

set (VIDTEX_SOURCES
   main.cpp
   launcher_rpi.c
   svp.c
   vidtex.c)
add_executable(vidtex ${VIDTEX_SOURCES})
target_link_libraries(vidtex GLESv2 EGL m bcm_host mmal_core mmal_components mmal_util mmal_vc_client vcos)


# raspistill/raspivid/raspiyuv

SET(COMPILE_DEFINITIONS -Werror)

include_directories(${CMAKE_SOURCE_DIR}/host_applications/linux/libs/bcm_host/include)

add_executable(raspistill RaspiCamControl.c  RaspiCLI.c  RaspiPreview.c  RaspiStill.c  RaspiTex.c RaspiTexUtil.c teapot.c models.c square.c mirror.c)
add_executable(raspiyuv   RaspiCamControl.c  RaspiCLI.c  RaspiPreview.c  RaspiStillYUV.c)
add_executable(raspivid   RaspiCamControl.c  RaspiCLI.c  RaspiPreview.c  RaspiVid.c)

target_link_libraries(raspistill mmal_core mmal_util mmal_vc_client vcos bcm_host GLESv2 EGL m)
target_link_libraries(raspiyuv   mmal_core mmal_util mmal_vc_client vcos bcm_host)
target_link_libraries(raspivid   mmal_core mmal_util mmal_vc_client vcos bcm_host)

install(TARGETS raspistill raspiyuv raspivid RUNTIME DESTINATION bin)

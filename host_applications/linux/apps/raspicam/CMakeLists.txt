
# raspistill/raspivid/raspiyuv

SET(COMPILE_DEFINITIONS -Werror)

add_executable(raspistill RaspiCamControl.c  RaspiCLI.c  RaspiPreview.c  RaspiStill.c)
add_executable(raspiyuv   RaspiCamControl.c  RaspiCLI.c  RaspiPreview.c  RaspiStillYUV.c)
add_executable(raspivid   RaspiCamControl.c  RaspiCLI.c  RaspiPreview.c  RaspiVid.c)

target_link_libraries(raspistill mmal_core mmal_util mmal_vc_client vcos)
target_link_libraries(raspiyuv   mmal_core mmal_util mmal_vc_client vcos)
target_link_libraries(raspivid   mmal_core mmal_util mmal_vc_client vcos)

install(TARGETS raspistill raspiyuv raspivid RUNTIME DESTINATION bin)

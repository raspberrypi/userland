
add_definitions(-Wall -Werror)

add_library(mmal SHARED util/mmal_util.c)

add_subdirectory(core)
add_subdirectory(util)
add_subdirectory(vc)

#add_subdirectory(test/lua)
#add_subdirectory(test/standalone)

if(BUILD_MMAL_COMPONENTS)
add_subdirectory(components)
target_link_libraries(mmal mmal_core mmal_util mmal_vc_client vcos mmal_components)
else(BUILD_MMAL_COMPONENTS)
target_link_libraries(mmal mmal_core mmal_util mmal_vc_client vcos)
endif(BUILD_MMAL_COMPONENTS)

install(TARGETS mmal DESTINATION lib)
install(FILES
   mmal.h
   mmal_buffer.h
   mmal_clock.h
   mmal_common.h
   mmal_component.h
   mmal_encodings.h
   mmal_events.h
   mmal_format.h
   mmal_logging.h
   mmal_metadata.h
   mmal_parameters.h
   mmal_parameters_audio.h
   mmal_parameters_camera.h
   mmal_parameters_clock.h
   mmal_parameters_common.h
   mmal_parameters_video.h
   mmal_pool.h mmal_port.h
   mmal_queue.h
   mmal_types.h
   DESTINATION include/interface/mmal
)

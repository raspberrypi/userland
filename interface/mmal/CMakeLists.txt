add_definitions(-Wall -Werror)
add_definitions(-DENABLE_MMAL_VCSM)

# Make it Android compatible
# OMX doesn't communicate with VC through OMX so the alignment
# mismatch doesn't matter there
remove_definitions(-DOMX_SKIP64BIT)

include_directories ( ../../host_applications/linux/libs/sm )

add_library(mmal SHARED
 util/mmal_util.c
 util/mmal_il.c
 util/mmal_connection.c
 util/mmal_graph.c
 util/mmal_list.c
 util/mmal_param_convert.c
 util/mmal_util_params.c
 util/mmal_component_wrapper.c
 util/mmal_util_rational.c
 core/mmal_format.c
 core/mmal_port.c
 core/mmal_port_clock.c
 core/mmal_component.c
 core/mmal_buffer.c
 core/mmal_queue.c
 core/mmal_pool.c
 core/mmal_events.c
 core/mmal_logging.c
 core/mmal_clock.c
 vc/mmal_vc_client.c
 vc/mmal_vc_shm.c
 vc/mmal_vc_api.c
 vc/mmal_vc_opaque_alloc.c
 vc/mmal_vc_msgnames.c
 vc/mmal_vc_api_drm.c
 components/null_sink.c
 components/passthrough.c
 components/scheduler.c
 components/splitter.c
 components/copy.c
 components/artificial_camera.c
 components/aggregator.c
 components/clock.c
 components/spdif.c
 openmaxil/mmalomx_core.c
 openmaxil/mmalomx_logging.c
 openmaxil/mmalomx_commands.c
 openmaxil/mmalomx_buffer.c
 openmaxil/mmalomx_marks.c
 openmaxil/mmalomx_roles.c
 openmaxil/mmalomx_parameters.c
 openmaxil/mmalomx_registry.c
 openmaxil/mmalomx_util_params.c
 openmaxil/mmalomx_util_params_audio.c
 openmaxil/mmalomx_util_params_video.c
 openmaxil/mmalomx_util_params_camera.c
 openmaxil/mmalomx_util_params_misc.c
)

target_link_libraries(mmal log vchiq_arm vcos vcsm)

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

add_executable(mmal_vc_diag vc/mmal_vc_diag.c)
target_link_libraries(mmal_vc_diag mmal debug_sym vcos)
install(TARGETS mmal_vc_diag RUNTIME DESTINATION bin)

add_subdirectory(client)
add_subdirectory(test)

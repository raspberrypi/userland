# linux apps

add_subdirectory(libs/bcm_host)
add_subdirectory(apps/gencmd)
add_subdirectory(apps/tvservice)
add_subdirectory(apps/raspicam)

if(ALL_APPS)
 add_subdirectory(apps/vcdbg)
 add_subdirectory(apps/edid_parser)
 add_subdirectory(apps/hello_pi)
endif()


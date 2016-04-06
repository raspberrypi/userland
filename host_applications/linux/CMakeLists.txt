# linux apps

add_subdirectory(libs/bcm_host)
add_subdirectory(apps/gencmd)
add_subdirectory(apps/tvservice)
add_subdirectory(apps/vcmailbox)
add_subdirectory(apps/raspicam)
add_subdirectory(libs/sm)
add_subdirectory(apps/smem)
add_subdirectory(libs/debug_sym)
add_subdirectory(apps/dtoverlay)

if(ALL_APPS)
 add_subdirectory(apps/vcdbg)
 # add_subdirectory(apps/smct)
 add_subdirectory(apps/edid_parser)
 add_subdirectory(apps/hello_pi)
endif()


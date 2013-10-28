cmake_minimum_required(VERSION 2.8)
project( FirePiCam )
SET(COMPILE_DEFINITIONS -Werror)
SET(FirePiCam_VERSION_MAJOR 0)
SET(FirePiCam_VERSION_MINOR 1)

include (InstallRequiredSystemLibraries)
SET(CPACK_RESOURCE_FILE_LICENSE  
     "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
SET(CPACK_PACKAGE_VERSION_MAJOR "${FirePiCam_VERSION_MAJOR}")
SET(CPACK_PACKAGE_VERSION_MINOR "${FirePiCam_VERSION_MINOR}")
include (CPack)

configure_file (
	"${PROJECT_SOURCE_DIR}/FirePiCam.h.in"
	"${PROJECT_BINARY_DIR}/FirePiCam.h"
	)

include_directories(/opt/vc/userland/host_applications/linux/libs/bcm_host/include)
include_directories(/opt/vc/userland/interface/vcos)
include_directories(/opt/vc/userland)
include_directories(/opt/vc/userland/interface/vcos/pthreads)
include_directories(/opt/vc/userland/interface/vmcs_host/linux)

add_library(firepicam SHARED RaspiCamControl.c RaspiCLI.c RaspiPreview.c FirePiCam.c)
target_link_libraries(firepicam /opt/vc/lib/libmmal_core.so /opt/vc/lib/libmmal_util.so /opt/vc/lib/libmmal_vc_client.so /opt/vc/lib/libvcos.so /opt/vc/lib/libbcm_host.so)

add_executable(firepistill FirePiStill.c)
target_link_libraries(firepistill /opt/vc/lib/libmmal_core.so /opt/vc/lib/libmmal_util.so /opt/vc/lib/libmmal_vc_client.so /opt/vc/lib/libvcos.so /opt/vc/lib/libbcm_host.so /home/pi/FirePiCam/libfirepicam.so)

install (TARGETS firepicam DESTINATION lib)
install (FILES FirePiCam.h DESTINATION include)

#OPENCV
#find_package( OpenCV REQUIRED )
#add_executable(fpc RaspiCamControl.c RaspiCLI.c RaspiPreview.c fpc.cpp)
#target_link_libraries(fpc /opt/vc/lib/libmmal_core.so /opt/vc/lib/libmmal_util.so /opt/vc/lib/libmmal_vc_client.so /opt/vc/lib/libvcos.so /opt/vc/lib/libbcm_host.so ${OpenCV_LIBS})


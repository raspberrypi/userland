cmake_minimum_required(VERSION 2.8)

get_filename_component (VIDEOCORE_ROOT ../../../.. ABSOLUTE)
include (${VIDEOCORE_ROOT}/makefiles/cmake/global_settings.cmake)

if (NOT WIN32)
   add_definitions(-Wall -Werror)
endif ()

include_directories (
   ${VIDEOCORE_HEADERS_BUILD_DIR}
   ${VIDEOCORE_ROOT}
   ${VIDEOCORE_ROOT}/opensrc/helpers/libfdt
   ${VIDEOCORE_ROOT}/helpers/dtoverlay
)

add_executable(dtoverlay dtoverlay_main.c utils.c)
target_link_libraries(dtoverlay dtovl fdt debug_sym vcos)

install(TARGETS dtoverlay RUNTIME DESTINATION bin)


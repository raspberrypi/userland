include_directories(${PROJECT_SOURCE_DIR}/host_applications/linux/libs/bcm_host/include)
add_executable(tvservice tvservice.c)
target_link_libraries(tvservice vchostif bcm_host)

install(TARGETS tvservice
        RUNTIME DESTINATION bin)
install(FILES tvservice.1 DESTINATION man/man1)

set(EXEC hello_wayland.bin)
set(SRCS triangle.c)

add_executable(${EXEC} ${SRCS})
target_link_libraries(${EXEC} ${HELLO_PI_LIBS} -lwayland-client -lwayland-egl)

install(TARGETS ${EXEC}
        RUNTIME DESTINATION bin)

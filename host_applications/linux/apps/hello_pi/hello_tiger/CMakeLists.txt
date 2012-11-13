set(EXEC hello_tiger.bin)
set(SRCS tiger.c main.c)

add_executable(${EXEC} ${SRCS})
target_link_libraries(${EXEC} ${HELLO_PI_LIBS})

add_definitions(-D__RASPBERRYPI__)

install(TARGETS ${EXEC}
        RUNTIME DESTINATION bin)

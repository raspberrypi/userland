add_executable(vcmailbox vcmailbox.c)
target_link_libraries(vcmailbox vchostif)

install(TARGETS vcmailbox
        RUNTIME DESTINATION bin)
install(FILES vcmailbox.1 DESTINATION man/man1)
install(FILES vcmailbox.7 raspiotp.7 raspirev.7 DESTINATION man/man7)

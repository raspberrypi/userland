
message(WARNING "usbdk: using stubbed out hostreq, HDMI buttons and gestures code")

add_library(vmcs_rpc_client
            vmcs_rpc_client/message_dispatch.c 
            test/stubs.c
            vmcs_rpc_client/platform.c)

target_link_libraries(vmcs_rpc_client vcos)

install(TARGETS vmcs_rpc_client DESTINATION lib)


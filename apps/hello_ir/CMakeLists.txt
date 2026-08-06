add_executable(hello_ir main.c)
target_link_libraries(hello_ir freewili2_bsp)
fw2_display_app(hello_ir
    VERSION 001
    DESCRIPTION "Infrared receive, decode, and transmit example")

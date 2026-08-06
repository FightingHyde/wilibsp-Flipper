add_executable(hello_dvi main.c)
target_link_libraries(hello_dvi freewili2_bsp)
fw2_display_app(hello_dvi
    VERSION 001
    DESCRIPTION "DVI output demonstration with a software on-screen display")

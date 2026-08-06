add_executable(hello_cc1101 main.c)
target_link_libraries(hello_cc1101 freewili2_bsp)
fw2_display_app(hello_cc1101
    VERSION 001
    DESCRIPTION "CC1101 sub-GHz radio receive and spectrum-monitor example")

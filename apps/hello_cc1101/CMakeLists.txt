add_executable(hello_cc1101 main.c)
target_link_libraries(hello_cc1101 freewili2_bsp)
fw2_display_app(hello_cc1101
    VERSION 002
    DESCRIPTION "CC1101 radio probe, RSSI scan, and GDO capture test")

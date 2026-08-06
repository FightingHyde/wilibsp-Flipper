add_executable(hello_charger main.c)
target_link_libraries(hello_charger freewili2_bsp)
fw2_display_app(hello_charger
    VERSION 001
    DESCRIPTION "Battery charger and USB-C status display")

add_executable(toggleled main.c)
target_link_libraries(toggleled freewili2_bsp onewili_fwgui)
fw2_display_app(toggleled
    VERSION 002
    DESCRIPTION "Visible external header GPIO toggle through OneWili")

add_executable(hello_keyboard main.c)
target_link_libraries(hello_keyboard freewili2_bsp)
fw2_display_app(hello_keyboard
    VERSION 001
    DESCRIPTION "Button and two-press chord keyboard input viewer")

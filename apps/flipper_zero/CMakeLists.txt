cmake_minimum_required(VERSION 3.20)

# Create the executable FIRST
add_executable(FlipperZero flipper_app.c)

# Then configure it with fw2_display_app
fw2_display_app(FlipperZero
    NAME "Flipper Zero"
    VERSION "111"
    DESCRIPTION "Flipper Zero compatible app"
)

# Link to the BSP library
target_link_libraries(FlipperZero PRIVATE freewili2_bsp)

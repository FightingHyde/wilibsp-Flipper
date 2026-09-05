cmake_minimum_required(VERSION 3.20)

fw2_display_app(FlipperZero
    NAME "Flipper Zero"
    VERSION "111"
    DESCRIPTION "Flipper Zero compatible app"
)

target_sources(FlipperZero PRIVATE flipper_app.c)

add_executable(hello_sensors main.c)
target_link_libraries(hello_sensors freewili2_bsp)
fw2_display_app(hello_sensors
    VERSION 001
    DESCRIPTION "Temperature, light, accelerometer, and magnetometer viewer")

add_executable(hello_sdcard main.c)
target_link_libraries(hello_sdcard freewili2_bsp onewili_fwgui)
pico_set_binary_type(hello_sdcard copy_to_ram)   # required: firmware runs from SRAM
pico_add_extra_outputs(hello_sdcard)             # .uf2 / .bin / .map

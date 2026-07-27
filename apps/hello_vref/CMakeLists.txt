# App target name MUST be the single token 'hello_vref' — `fw new-app <name>`
# rewrites every 'hello_vref' occurrence to the new app name.
add_executable(hello_vref main.c)
target_link_libraries(hello_vref freewili2_bsp onewili_fwgui hardware_adc)
pico_set_binary_type(hello_vref copy_to_ram)   # required: firmware runs from SRAM
pico_add_extra_outputs(hello_vref)             # .uf2 / .bin / .map

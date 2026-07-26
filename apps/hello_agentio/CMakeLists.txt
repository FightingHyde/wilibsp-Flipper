add_executable(hello_agentio main.c)
target_link_libraries(hello_agentio freewili2_bsp)
pico_set_binary_type(hello_agentio copy_to_ram)   # required: firmware runs from SRAM
pico_add_extra_outputs(hello_agentio)             # .uf2 / .bin / .map

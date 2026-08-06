# freewili2_bsp — shared board-support library (STATIC once it has sources).
add_library(freewili2_bsp STATIC
    common/uf2_info.c
    agentio/agentio.c
    agentio/agentio_rle.c
    agentio/agentio_shadow.c
    platform/board.c
    platform/ioexp.c
    platform/psram.c
    platform/spi_bus.c
    display/st7796.c
    display/font5x7.c
    display/dvi_osd.c
    display/hstx_dvi.c
    input/ft6336.c
    input/ft6336_map.c
    leds/led_color.c
    leds/ws2812_driver.c
    leds/led_ui.c
    audio/tone_gen.c
    audio/vu_meter.c
    audio/codec_nau88c10.c
    audio/audio_i2s_duplex.c
    audio/audio_capture.c
    radio/cc1101_regs.c
    radio/cc1101.c
    radio/gdo_capture.c
    radio/monitor_engine.c
    radio/ook_tx.c
    radio/scan_engine.c
    radio/capture_store.c
    pdm/pdm_capture.c
    dsp/cic.c
    dsp/dcblock.c
    sensors/sht40.c
    sensors/opt4001.c
    sensors/bmi323.c
    sensors/bmm350.c
    sensors/bmm350_comp.c
    ir/ir_protocols.c
    ir/ir_decode.c
    ir/ir_encode.c
    ir/ir_frame.c
    ir/ir_tx_pack.c
    ir/ir_file.c
    ir/ir_resolve.c
    ir/db_sort.c
    ir/ir_capture.c
    ir/ir_tx.c
    gfx/palette.c
    keyboard/fw2kb.c
    keyboard/fw2kb_hidmap.c
    input/uartkbd_parse.c
    input/uartkbd.c
    input/app_recovery.c
    input/picpwr_frame.c
    input/picpwr.c
    usbhost/usb_core.c
    usbhost/usb_hcd.c
    usbhost/usb_hub.c
    usbhost/usb_msc.c
    usbhost/usb_parse.c
    usbhost/msc_disk.c
    usbhost/usb_store.c
    third_party/fatfs/ff.c
    third_party/fatfs/ffunicode.c
    third_party/segger_rtt/SEGGER_RTT.c
    third_party/segger_rtt/SEGGER_RTT_printf.c)

target_include_directories(freewili2_bsp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/segger_rtt
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fatfs)

target_link_libraries(freewili2_bsp PUBLIC
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_dma
    hardware_pio
    hardware_clocks
    hardware_vreg
    hardware_psram
    hardware_pwm
    hardware_watchdog)

target_compile_definitions(freewili2_bsp PUBLIC FW2_AGENTIO=$<BOOL:${FW2_AGENTIO}>)

# GDO0 = GPIO32; PIO must use the GPIO base window 16..47 to reach it.
target_compile_definitions(freewili2_bsp PUBLIC PICO_PIO_USE_GPIO_BASE=1)

# Generate the WS2812 PIO header into the build tree for ws2812_driver.c.
pico_generate_pio_header(freewili2_bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/leds/ws2812.pio)

# Generate the I2S full-duplex PIO header into the build tree for audio_i2s_duplex.c.
pico_generate_pio_header(freewili2_bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/i2s_duplex.pio)

# Generate the GDO0 capture PIO header into the build tree for gdo_capture.c.
pico_generate_pio_header(freewili2_bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/radio/gdo_capture.pio)

# Generate the PDM capture PIO header into the build tree for pdm_capture.c.
pico_generate_pio_header(freewili2_bsp
    ${CMAKE_CURRENT_SOURCE_DIR}/pdm/pdm_capture.pio)

# Generate the IR PIO headers into the build tree for ir_capture.c and ir_tx.c.
pico_generate_pio_header(freewili2_bsp ${CMAKE_CURRENT_SOURCE_DIR}/ir/ir_capture.pio)
pico_generate_pio_header(freewili2_bsp ${CMAKE_CURRENT_SOURCE_DIR}/ir/ir_tx.pio)

set(FW2_TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/../tools" CACHE PATH
    "Directory containing FW2 build validation tools")

function(fw2_display_app target)
    cmake_parse_arguments(PARSE_ARGV 1 APP "" "NAME;VERSION;DESCRIPTION" "")
    if(NOT APP_NAME)
        set(APP_NAME "${target}")
    endif()
    if(NOT APP_VERSION MATCHES "^[0-9][0-9][0-9]$")
        message(FATAL_ERROR
            "${target}: VERSION must be exactly three digits (000-999)")
    endif()
    if(NOT APP_DESCRIPTION)
        message(FATAL_ERROR
            "${target}: DESCRIPTION is required for the App Explorer")
    endif()
    string(LENGTH "${APP_NAME}" _name_len)
    string(LENGTH "${APP_DESCRIPTION}" _description_len)
    if(_name_len GREATER 31)
        message(FATAL_ERROR "${target}: NAME exceeds 31 ASCII bytes")
    endif()
    if(_description_len GREATER 127)
        message(FATAL_ERROR "${target}: DESCRIPTION exceeds 127 ASCII bytes")
    endif()

    string(REGEX REPLACE "^0+([0-9])" "\\1" _version_number
           "${APP_VERSION}")
    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_uf2_info")
    add_custom_command(
        OUTPUT "${_dir}/${target}_uf2_info.c"
        COMMAND ${Python3_EXECUTABLE} "${FW2_TOOLS_DIR}/gen_uf2_info.py"
                --out "${_dir}/${target}_uf2_info.c"
                --version "${_version_number}"
                --name "${APP_NAME}"
                --description "${APP_DESCRIPTION}"
        DEPENDS "${FW2_TOOLS_DIR}/gen_uf2_info.py"
        COMMENT "Generating FW2 app metadata for ${target}"
        VERBATIM)
    target_sources(${target} PRIVATE "${_dir}/${target}_uf2_info.c")
    target_link_options(${target} PRIVATE
        -Wl,--undefined=fw2app_uf2_info)

    # /apps images execute only from SRAM or PSRAM; never target QSPI flash.
    pico_set_binary_type(${target} copy_to_ram)
    pico_add_extra_outputs(${target})
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} "${FW2_TOOLS_DIR}/check_uf2_info.py"
                --bin "$<TARGET_FILE_DIR:${target}>/${target}.bin"
        VERBATIM)
endfunction()

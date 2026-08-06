add_executable(hello_psram_exec startup.S bootstrap.c main.c)

# Loader-launched PSRAM image: linking the normal SDK CRT would repeat
# clock/reset/PSRAM initialization inherited from the DISPLAY loader.
target_compile_options(hello_psram_exec PRIVATE
    -ffreestanding -fno-builtin -fdata-sections -ffunction-sections)
target_link_options(hello_psram_exec PRIVATE
    -nostartfiles -nostdlib LINKER:--build-id=none LINKER:--gc-sections)
target_link_options(hello_psram_exec PRIVATE
    LINKER:-T,${CMAKE_CURRENT_LIST_DIR}/psram_exec.ld)
pico_add_link_depend(hello_psram_exec
    ${CMAKE_CURRENT_LIST_DIR}/psram_exec.ld)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(HELLO_PSRAM_BIN ${CMAKE_CURRENT_BINARY_DIR}/hello_psram_exec.bin)
set(HELLO_PSRAM_UF2 ${CMAKE_CURRENT_BINARY_DIR}/hello_psram_exec.uf2)
add_custom_command(TARGET hello_psram_exec POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:hello_psram_exec> ${HELLO_PSRAM_BIN}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/make_uf2.py
            ${HELLO_PSRAM_BIN} ${HELLO_PSRAM_UF2}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/verify_layout.py
            $<TARGET_FILE:hello_psram_exec>
            ${CMAKE_OBJDUMP}
            ${HELLO_PSRAM_UF2}
    VERBATIM
    COMMENT "Verifying PSRAM image and SRAM bootstrap layout")

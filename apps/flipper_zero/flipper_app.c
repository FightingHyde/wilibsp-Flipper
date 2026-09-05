#include "fw2.h"
#include <FreeRTOS.h>
#include <task.h>

static TaskHandle_t furi_task = NULL;

static void furi_task_entry(void* pvParameters) {
    (void)pvParameters;
    while(1) { 
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

int flipper_app_main(fw2_app_ctx_t* ctx) {
    (void)ctx;
    xTaskCreateAffinitySet(furi_task_entry, "FuriOS", 8192, NULL,
        configMAX_PRIORITIES - 2, (UBaseType_t)1, &furi_task);
    return 0;
}

void flipper_app_tick(fw2_app_ctx_t* ctx) {
    if(fw2_input_pressed(FW2_BTN_HOME) && fw2_input_held(FW2_BTN_HOME, 1000))
        fw2_app_quit(ctx);
}

void flipper_app_exit(void) {
    if(furi_task) vTaskDelete(furi_task);
}

FW2_APP_REGISTER(
    .name = "Flipper Zero",
    .version = "0.1.0",
    .author = "Community",
    .main = flipper_app_main,
    .tick = flipper_app_tick,
    .exit = flipper_app_exit,
);

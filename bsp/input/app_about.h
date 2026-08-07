#ifndef FW2_APP_ABOUT_H
#define FW2_APP_ABOUT_H

#include <stdint.h>

typedef void (*fw2_app_about_renderer_t)(const char *name, uint16_t version,
                                         const char *repository);

/* Register the surface used for the PAGE-hold About screen. */
void fw2_app_about_set_renderer(fw2_app_about_renderer_t renderer,
                                void (*restore)(void));
void fw2_app_about_use_lcd(void);

/* Called by the recovery contract after uartkbd_task(). */
void fw2_app_about_task(void);

#endif

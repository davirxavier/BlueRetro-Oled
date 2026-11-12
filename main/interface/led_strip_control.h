//
// Created by fdavi on 12/11/2026.
//

#ifndef BLUE_RETRO_DUAL_PIO_LED_STRIP_H
#define BLUE_RETRO_DUAL_PIO_LED_STRIP_H

#include "esp_err.h"

typedef enum {
    WS2812_STATUS_OFF,
    WS2812_STATUS_BOOTING,
    WS2812_STATUS_READY,
    WS2812_STATUS_PAIRING,
    WS2812_STATUS_CONNECTED,
    WS2812_STATUS_ERROR
} ws2812_status_t;

esp_err_t ws2812_status_init(void);
void ws2812_status_set(ws2812_status_t status);

#endif //BLUE_RETRO_DUAL_PIO_LED_STRIP_H

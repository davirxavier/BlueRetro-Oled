#include "led_strip_control.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WS2812_GPIO 18

static led_strip_handle_t led_strip;
static volatile ws2812_status_t current_status = WS2812_STATUS_BOOTING;
static TaskHandle_t led_task_handle = NULL;

static void led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void led_task(void *arg)
{
    ws2812_status_t previous_status = WS2812_STATUS_OFF;
    bool blink = false;

    while (true) {
        ws2812_status_t status = current_status;

        if (status != previous_status) {
            previous_status = status;
            blink = false;

            switch (status) {
                case WS2812_STATUS_OFF: led_set_color(0, 0, 0); break;
                case WS2812_STATUS_BOOTING: led_set_color(80, 0, 120); break;
                case WS2812_STATUS_READY: led_set_color(255, 100, 0); break;
                case WS2812_STATUS_CONNECTED: led_set_color(0, 80, 0); break;
                default: break;
            }
        }

        switch (status) {
            case WS2812_STATUS_PAIRING:
                blink = !blink;
                led_set_color(0, 0, blink ? 100 : 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;

            case WS2812_STATUS_ERROR:
                blink = !blink;
                led_set_color(blink ? 150 : 0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(150));
                break;

            default:
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                break;
        }
    }
}

esp_err_t ws2812_status_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    if (xTaskCreatePinnedToCore(led_task, "ws2812_task", 2048, NULL, 1, &led_task_handle, 0) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

void ws2812_status_set(ws2812_status_t status)
{
    current_status = status;

    if (led_task_handle != NULL) {
        xTaskNotifyGive(led_task_handle);
    }
}
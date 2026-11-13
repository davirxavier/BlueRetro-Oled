#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "int_button.h"
#include "oled_display.h"
#include <stdbool.h>
#include <stdint.h>

#define BUTTON1_GPIO GPIO_NUM_12
#define BUTTON2_GPIO GPIO_NUM_15
#define BUTTON_DEBOUNCE_MS 40
#define BUTTON_LONG_PRESS_MS 600
#define BUTTON_COOLDOWN_MS 100
#define BUTTON_POLL_MS 10

typedef struct {
    gpio_num_t pin;
    uint8_t id;
    bool pressed;
    bool last_sample;
    bool long_fired;
    bool has_last_action;
    TickType_t change_tick;
    TickType_t press_tick;
    TickType_t last_action_tick;
} button_state_t;

static button_state_t buttons[] = {
    {.pin = BUTTON1_GPIO, .id = 1},
    {.pin = BUTTON2_GPIO, .id = 2}
};

static bool button_cooldown_ready(button_state_t *button, TickType_t now)
{
    if (button->has_last_action && (now - button->last_action_tick) < pdMS_TO_TICKS(BUTTON_COOLDOWN_MS)) return false;
    button->last_action_tick = now;
    button->has_last_action = true;
    return true;
}

static void button_handle_event(button_state_t *button, bool long_press)
{
    TickType_t now = xTaskGetTickCount();
    if (!button_cooldown_ready(button, now)) return;

    if (button->id == 1) {
        oled_cmd(long_press ? OLED_BACK : OLED_UP);
    } else {
        oled_cmd(long_press ? OLED_CONFIRM : OLED_DOWN);
    }
}

static void button_task(void *arg)
{
    TickType_t now;

    while (true) {
        now = xTaskGetTickCount();

        for (unsigned int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
            button_state_t *button = &buttons[i];
            bool sample = gpio_get_level(button->pin) == 0;

            if (sample != button->last_sample) {
                button->last_sample = sample;
                button->change_tick = now;
            }

            if (sample != button->pressed && (now - button->change_tick) >= pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
                button->pressed = sample;

                if (button->pressed) {
                    button->press_tick = now;
                    button->long_fired = false;
                } else if (!button->long_fired) {
                    button_handle_event(button, false);
                }
            }

            if (button->pressed && !button->long_fired && (now - button->press_tick) >= pdMS_TO_TICKS(BUTTON_LONG_PRESS_MS)) {
                button->long_fired = true;
                button_handle_event(button, true);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

esp_err_t button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON1_GPIO) | (1ULL << BUTTON2_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;

    TickType_t now = xTaskGetTickCount();

    for (unsigned int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        bool pressed = gpio_get_level(buttons[i].pin) == 0;
        buttons[i].pressed = pressed;
        buttons[i].last_sample = pressed;
        buttons[i].change_tick = now;
        buttons[i].press_tick = now;
    }

    if (xTaskCreate(button_task, "button_task", 3072, NULL, 2, NULL) != pdPASS) return ESP_ERR_NO_MEM;

    return ESP_OK;
}

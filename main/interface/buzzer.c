
#include "buzzer.h"
#include <string.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define BUZZER_GPIO GPIO_NUM_4
#define BUZZER_TIMER LEDC_TIMER_0
#define BUZZER_CHANNEL LEDC_CHANNEL_0
#define BUZZER_SPEED LEDC_LOW_SPEED_MODE
#define BUZZER_RESOLUTION LEDC_TIMER_10_BIT
#define BUZZER_DUTY 512
#define BUZZER_QUEUE_LENGTH 1
#define BUZZER_TASK_STACK 3072
#define BUZZER_TASK_PRIORITY 1
#define BUZZER_NOTE_GAP_MS 20
#define BUZZER_CHECK_INTERVAL_MS 10

typedef struct {
    const buzzer_note_t *notes;
    uint16_t count;
} buzzer_song_data_t;

typedef enum {
    BUZZER_ACTION_PLAY,
    BUZZER_ACTION_STOP
} buzzer_action_t;

typedef struct {
    buzzer_action_t action;
    buzzer_note_t notes[64];
    uint16_t count;
} buzzer_request_t;

static QueueHandle_t buzzer_queue;
static TaskHandle_t buzzer_task_handle;
static volatile bool buzzer_initialized = false;

static const buzzer_note_t song_boot[] = {
    {1319, 110}, {1568, 110}, {2637, 110},
    {2093, 110}, {2349, 110}, {3136, 110}
};

static const buzzer_note_t song_ready[] = {
    {587, 90}, {659, 90}, {784, 90},
    {880, 90}, {988, 90}, {1175, 360}
};

static const buzzer_note_t song_pairing[] = {
    {392, 160}, {415, 160}, {440, 160}, {466, 550}
};

static const buzzer_note_t song_connected[] = {
    {784, 140}, {740, 140}, {622, 140}, {440, 140},
    {415, 140}, {659, 140}, {831, 140}, {1047, 420}
};

static const buzzer_note_t song_disconnected[] = {
    {185, 90}, {0, 20}, {147, 150}
};

static const buzzer_note_t song_error[] = {
    {880, 60}, {0, 15}, {622, 90}, {0, 70},
    {880, 60}, {0, 15}, {622, 90}
};

static const buzzer_note_t song_button[] = {
    {988, 100}, {1319, 450}
};

static const buzzer_note_t song_change[] = {
    {1319, 70}, {1760, 140}
};

static const buzzer_song_data_t songs[BUZZER_SONG_MAX] = {
    [BUZZER_SONG_BOOT] = {song_boot, sizeof(song_boot) / sizeof(song_boot[0])},
    [BUZZER_SONG_READY] = {song_ready, sizeof(song_ready) / sizeof(song_ready[0])},
    [BUZZER_SONG_PAIRING] = {song_pairing, sizeof(song_pairing) / sizeof(song_pairing[0])},
    [BUZZER_SONG_CONNECTED] = {song_connected, sizeof(song_connected) / sizeof(song_connected[0])},
    [BUZZER_SONG_DISCONNECTED] = {song_disconnected, sizeof(song_disconnected) / sizeof(song_disconnected[0])},
    [BUZZER_SONG_ERROR] = {song_error, sizeof(song_error) / sizeof(song_error[0])},
    [BUZZER_SONG_BUTTON] = {song_button, sizeof(song_button) / sizeof(song_button[0])},
    [BUZZER_SONG_CHANGE] = {song_change, sizeof(song_change) / sizeof(song_change[0])}
};

static void buzzer_silence(void)
{
    ledc_set_duty(BUZZER_SPEED, BUZZER_CHANNEL, 0);
    ledc_update_duty(BUZZER_SPEED, BUZZER_CHANNEL);
}

static void buzzer_tone(uint16_t frequency)
{
    if (frequency == 0) {
        buzzer_silence();
        return;
    }
    ledc_set_freq(BUZZER_SPEED, BUZZER_TIMER, frequency);
    ledc_set_duty(BUZZER_SPEED, BUZZER_CHANNEL, BUZZER_DUTY);
    ledc_update_duty(BUZZER_SPEED, BUZZER_CHANNEL);
}

/* Wait for a duration, but allow a new request to interrupt it. */
static bool buzzer_wait(uint16_t duration_ms, buzzer_request_t *next)
{
    TickType_t remaining = pdMS_TO_TICKS(duration_ms);
    if (duration_ms > 0 && remaining == 0) remaining = 1;

    TickType_t interval = pdMS_TO_TICKS(BUZZER_CHECK_INTERVAL_MS);
    if (interval == 0) interval = 1;

    while (remaining > 0) {
        TickType_t wait = remaining < interval ? remaining : interval;
        if (xQueueReceive(buzzer_queue, next, wait) == pdTRUE) {
            return true;
        }
        remaining -= wait;
    }
    return false;
}

/* Returns true if a new request interrupted playback. */
static bool buzzer_play_sequence(const buzzer_note_t *notes, uint16_t count, buzzer_request_t *next)
{
    for (uint16_t i = 0; i < count; i++) {
        buzzer_tone(notes[i].frequency);

        if (buzzer_wait(notes[i].duration, next)) {
            buzzer_silence();
            return true;
        }

        buzzer_silence();

        if (notes[i].frequency && i + 1 < count) {
            if (buzzer_wait(BUZZER_NOTE_GAP_MS, next)) {
                buzzer_silence();
                return true;
            }
        }
    }

    buzzer_silence();
    return false;
}

static void buzzer_task(void *arg)
{
    buzzer_request_t request;

    while (true) {
        if (xQueueReceive(buzzer_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        while (request.action == BUZZER_ACTION_PLAY) {
            buzzer_request_t next;

            if (buzzer_play_sequence(request.notes, request.count, &next)) {
                request = next;
            } else {
                break;
            }
        }

        buzzer_silence();
    }
}

esp_err_t buzzer_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = BUZZER_SPEED,
        .duty_resolution = BUZZER_RESOLUTION,
        .timer_num = BUZZER_TIMER,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) return ret;

    ledc_channel_config_t channel_cfg = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = BUZZER_SPEED,
        .channel = BUZZER_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_TIMER,
        .duty = 0,
        .hpoint = 0
    };

    ret = ledc_channel_config(&channel_cfg);
    if (ret != ESP_OK) return ret;

    buzzer_queue = xQueueCreate(BUZZER_QUEUE_LENGTH, sizeof(buzzer_request_t));
    if (!buzzer_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreatePinnedToCore(buzzer_task, "buzzer_task", BUZZER_TASK_STACK, NULL, BUZZER_TASK_PRIORITY, &buzzer_task_handle, 0) != pdPASS) {
        vQueueDelete(buzzer_queue);
        buzzer_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    buzzer_initialized = true;
    return ESP_OK;
}

esp_err_t buzzer_play(buzzer_song_t song)
{
    if (!buzzer_initialized) return ESP_ERR_INVALID_STATE;
    if ((unsigned)song >= BUZZER_SONG_MAX) return ESP_ERR_INVALID_ARG;

    buzzer_request_t request = {
        .action = BUZZER_ACTION_PLAY,
        .count = songs[song].count
    };

    if (request.count == 0 || request.count > 64) return ESP_ERR_INVALID_SIZE;

    memcpy(request.notes, songs[song].notes, request.count * sizeof(buzzer_note_t));

    return xQueueOverwrite(buzzer_queue, &request) == pdTRUE ? ESP_OK : ESP_FAIL;
}

void buzzer_stop(void)
{
    if (!buzzer_initialized) return;

    buzzer_request_t request = {
        .action = BUZZER_ACTION_STOP,
        .count = 0
    };

    xQueueOverwrite(buzzer_queue, &request);
}

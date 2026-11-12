
#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t frequency; // Hz; 0 = silence
    uint16_t duration;  // milliseconds
} buzzer_note_t;

typedef enum {
    BUZZER_SONG_BOOT,
    BUZZER_SONG_READY,
    BUZZER_SONG_PAIRING,
    BUZZER_SONG_CONNECTED,
    BUZZER_SONG_DISCONNECTED,
    BUZZER_SONG_ERROR,
    BUZZER_SONG_BUTTON,
    BUZZER_SONG_CONFIRM,
    BUZZER_SONG_BACK,
    BUZZER_SONG_MAX
} buzzer_song_t;

esp_err_t buzzer_init(void);
esp_err_t buzzer_play(buzzer_song_t song);
esp_err_t buzzer_play_notes(const buzzer_note_t *notes, uint16_t count);
void buzzer_stop(void);

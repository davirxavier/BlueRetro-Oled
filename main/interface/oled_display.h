//
// Created by fdavi on 12/11/2026.
//

#ifndef BLUE_RETRO_DUAL_PIO_OLED_DISPLAY_H
#define BLUE_RETRO_DUAL_PIO_OLED_DISPLAY_H

#include <stdint.h>
#include "adapter/adapter.h"
#include "buzzer.h"

#define OLED_MAX_PLAYERS 4

extern const char *oled_type_names[BT_TYPE_MAX];
extern const char *oled_subtype_names[BT_SUBTYPE_MAX];
extern const char *oled_name_unk;

typedef struct {
    bool connected;
    const char *name;
    uint8_t player;
    struct bt_data_base *bt_data_base;
} oled_player_t;

typedef struct {
    bool pairing;
    int pairingIndex;
    oled_player_t players[BT_MAX_DEV];
} oled_status_t;

enum
{
    OLED_UP = 0,
    OLED_DOWN,
    OLED_CONFIRM,
    OLED_BACK,
    OLED_CMD_MAX,
};

void oled_task(void *arg);
void oled_cmd(uint8_t cmd);

#endif //BLUE_RETRO_DUAL_PIO_OLED_DISPLAY_H

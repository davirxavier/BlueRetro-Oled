//
// Created by fdavi on 12/11/2026.
//

#ifndef BLUE_RETRO_DUAL_PIO_OLED_DISPLAY_H
#define BLUE_RETRO_DUAL_PIO_OLED_DISPLAY_H

#ifndef OLED_LANG
#define OLED_LANG EN
#endif

#include <stdint.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "buzzer.h"

#define OLED_MAX_PLAYERS 4

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

typedef enum
{
    OLED_UP = 0,
    OLED_DOWN,
    OLED_CONFIRM,
    OLED_BACK,
    OLED_REFRESH,
    OLED_CMD_MAX,
} oled_cmd_t;

enum
{
    OLED_SCREEN_HOME = 0,
    OLED_SCREEN_SETTINGS,
    OLED_SCREEN_DIAGNOSTICS,
    OLED_SCREEN_PLAYER_SETTINGS,
    OLED_SCREEN_BUTTON_SETTINGS,
    OLED_SCREEN_STICK_SETTINGS,
    OLED_SCREEN_VIBRATION_SETTINGS,

    // DIAG
    OLED_SCREEN_BUTTON_TEST,
    OLED_SCREEN_STICK_TEST,
    OLED_SCREEN_VIBRATION_TEST,
    OLED_SCREEN_BATTERY_TEST,

    // PLAYER
    OLED_SCREEN_RECONNECT,
    OLED_SCREEN_DISCONNECT,
    OLED_SCREEN_ASSIGN_PLAYER,
    OLED_SCREEN_AUTO_ASSIGN_PLAYER,

    OLED_SCREEN_MAX,
};

enum
{
    OLED_OPTION_NONE = 0,
    OLED_OPTION_TYPE_NUM,
    OLED_OPTION_TYPE_OPT,
    OLED_OPTION_TYPE_NAV,
    OLED_OPTION_TYPE_MAX,
};

enum
{
    OLED_OPTION_EVENT_INVALID = 0,
    OLED_OPTION_EVENT_RECONNECT,
    OLED_OPTION_EVENT_DISCONNECT,
    OLED_OPTION_EVENT_ASSIGN_LOCK,
    OLED_OPTION_EVENT_CONFIRM,
    OLED_OPTION_EVENT_MAX,
};

typedef struct
{
    uint8_t screen;
    uint8_t type;
    uint8_t navigation_screen;
    uint8_t step;
    uint8_t min;
    uint8_t max;
    uint8_t def;
    const char suffix[3];
    const char *name;
    const char **option_list;
    size_t option_list_len;
    uint8_t on_confirm_event;
    uint8_t (*getter)(void);
    void (*setter)(uint8_t);
} oled_screen_option_t;

extern oled_screen_option_t oled_screen_options[];
extern const size_t oled_screen_options_count;
extern const char *oled_type_names[BT_TYPE_MAX];
extern const char *oled_subtype_names[BT_SUBTYPE_MAX];
extern const char *oled_name_unk;
extern const char *oled_screen_names[OLED_SCREEN_MAX];
extern uint8_t oled_current_slot;

void oled_config_save(void);
void oled_task(void *arg);
void oled_cmd(oled_cmd_t cmd);
void oled_update_vals();

#endif //BLUE_RETRO_DUAL_PIO_OLED_DISPLAY_H

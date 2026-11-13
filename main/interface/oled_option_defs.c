#include <esp_log.h>

#include "oled_display.h"
#include "oled_option_text.h"
#include "adapter/config.h"
#include "adapter/adapter.h"

static const uint8_t left_stick_src[] = {PAD_LX_LEFT, PAD_LX_RIGHT, PAD_LY_UP, PAD_LY_DOWN};
static const uint8_t left_stick_src_size = sizeof(left_stick_src) / sizeof(left_stick_src[0]);
static const uint8_t right_stick_src[] = {PAD_RX_LEFT, PAD_RX_RIGHT, PAD_RY_UP, PAD_RY_DOWN};
static const uint8_t right_stick_src_size = sizeof(right_stick_src) / sizeof(right_stick_src[0]);

static uint8_t oled_get_stick_deadzone(uint8_t input_id, const uint8_t *src, size_t count)
{
    if (input_id >= WIRED_MAX_DEV) return 0;
    struct in_cfg *input = &config.in_cfg[input_id];
    uint8_t map_size = input->map_size;
    if (map_size > ADAPTER_MAPPING_MAX) map_size = ADAPTER_MAPPING_MAX;
    for (size_t i = 0; i < count; i++)
    {
        for (uint8_t j = 0; j < map_size; j++)
        {
            if (input->map_cfg[j].src_btn == src[i]) return input->map_cfg[j].perc_deadzone;
        }
    }
    return 0;
}

static void oled_set_stick_deadzone(uint8_t input_id, const uint8_t *src, size_t count, uint8_t value)
{
    if (input_id >= WIRED_MAX_DEV || value > 100) return;
    struct in_cfg *input = &config.in_cfg[input_id];
    uint8_t map_size = input->map_size;
    if (map_size > ADAPTER_MAPPING_MAX) map_size = ADAPTER_MAPPING_MAX;
    for (size_t i = 0; i < count; i++)
    {
        for (uint8_t j = 0; j < map_size; j++)
        {
            if (input->map_cfg[j].src_btn == src[i]) input->map_cfg[j].perc_deadzone = value;
        }
    }
}

static uint8_t oled_get_stick_max(uint8_t input_id, const uint8_t *src, size_t count)
{
    if (input_id >= WIRED_MAX_DEV) return 0;
    struct in_cfg *input = &config.in_cfg[input_id];
    uint8_t map_size = input->map_size;
    if (map_size > ADAPTER_MAPPING_MAX) map_size = ADAPTER_MAPPING_MAX;
    for (size_t i = 0; i < count; i++)
    {
        for (uint8_t j = 0; j < map_size; j++)
        {
            if (input->map_cfg[j].src_btn == src[i]) return input->map_cfg[j].perc_max;
        }
    }
    return 0;
}

static void oled_set_stick_max(uint8_t input_id, const uint8_t *src, size_t count, uint8_t value)
{
    if (input_id >= WIRED_MAX_DEV || value > 100) return;
    struct in_cfg *input = &config.in_cfg[input_id];
    uint8_t map_size = input->map_size;
    if (map_size > ADAPTER_MAPPING_MAX) map_size = ADAPTER_MAPPING_MAX;
    for (size_t i = 0; i < count; i++)
    {
        for (uint8_t j = 0; j < map_size; j++)
        {
            if (input->map_cfg[j].src_btn == src[i]) input->map_cfg[j].perc_max = value;
        }
    }
}

static uint8_t get_ldeadzone()
{
    return oled_get_stick_deadzone(oled_current_slot, left_stick_src, left_stick_src_size);
}

static void set_ldeadzone(uint8_t value)
{
    ESP_LOGI("sdad", "SAdasd");
    oled_set_stick_deadzone(oled_current_slot, left_stick_src, left_stick_src_size, value);
}

static uint8_t get_rdeadzone()
{
    return oled_get_stick_deadzone(oled_current_slot, right_stick_src, right_stick_src_size);
}

static void set_rdeadzone(uint8_t value)
{
    oled_set_stick_deadzone(oled_current_slot, right_stick_src, right_stick_src_size, value);
}

static uint8_t get_lmax()
{
    return oled_get_stick_max(oled_current_slot, left_stick_src, left_stick_src_size);
}

static void set_lmax(uint8_t value)
{
    oled_set_stick_max(oled_current_slot, left_stick_src, left_stick_src_size, value);
}

static uint8_t get_rmax()
{
    return oled_get_stick_max(oled_current_slot, right_stick_src, right_stick_src_size);
}

static void set_rmax(uint8_t value)
{
    oled_set_stick_max(oled_current_slot, right_stick_src, right_stick_src_size, value);
}


static const char *confirm_options[] = {"Y", "N"};
static const size_t confirm_options_len = sizeof(confirm_options) / sizeof(confirm_options[0]);
static const char *assign_options[] = {"P1", "P2", "P3", "P4"};
static const size_t assign_options_len = sizeof(assign_options) / sizeof(assign_options[0]);

oled_screen_option_t oled_screen_options[] = {
    { // EMPTY
        .screen = 255,
        .type = 0,
        .navigation_screen = 0,
        .step = 0,
        .min = 0,
        .max = 0,
        .def = 0,
        .option_list = NULL,
        .option_list_len = 0,
    },

    // Settings
    { // DIAG
        .screen = OLED_SCREEN_SETTINGS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_DIAGNOSTICS,
        .name = oled_option_name_diagnostics,
    },
    { // PLAYER
        .screen = OLED_SCREEN_SETTINGS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_PLAYER_SETTINGS,
        .name = oled_option_name_player,
    },
    { // STICK
        .screen = OLED_SCREEN_SETTINGS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_STICK_SETTINGS,
        .name = oled_option_name_stick,
    },
    { // BUTTON
        .screen = OLED_SCREEN_SETTINGS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_BUTTON_SETTINGS,
        .name = oled_option_name_button,
    },
    { // VIBRATION
        .screen = OLED_SCREEN_SETTINGS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_VIBRATION_SETTINGS,
        .name = oled_option_name_vibration,
    },

    // DIAG
    { // BTN
        .screen = OLED_SCREEN_DIAGNOSTICS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_BUTTON_TEST,
        .name = oled_option_name_button_test,
    },
    { // STICK
        .screen = OLED_SCREEN_DIAGNOSTICS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_STICK_TEST,
        .name = oled_option_name_stick_test,
    },
    { // VIBR
        .screen = OLED_SCREEN_DIAGNOSTICS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_VIBRATION_TEST,
        .name = oled_option_name_vibration_test,
    },
    { // BATTERY
        .screen = OLED_SCREEN_DIAGNOSTICS,
        .type = OLED_OPTION_TYPE_NAV,
        .navigation_screen = OLED_SCREEN_BATTERY_TEST,
        .name = oled_option_name_low_battery,
    },

    // PLAYER
    { // RECONNECT
        .screen = OLED_SCREEN_PLAYER_SETTINGS,
        .type = OLED_OPTION_NONE,
        .name = oled_option_name_reconnect,
        .on_confirm_event = OLED_OPTION_EVENT_RECONNECT,
    },
    { // DISCONNECT
        .screen = OLED_SCREEN_PLAYER_SETTINGS,
        .type = OLED_OPTION_NONE,
        .name = oled_option_name_disconnect,
        .on_confirm_event = OLED_OPTION_EVENT_DISCONNECT,
    },
    { // ASSIGN
        .screen = OLED_SCREEN_PLAYER_SETTINGS,
        .type = OLED_OPTION_TYPE_OPT,
        .name = oled_option_name_assign_player,
        .option_list = assign_options,
        .option_list_len = assign_options_len,
        .on_confirm_event = OLED_OPTION_EVENT_CONFIRM,
    },
    { // LOCK
        .screen = OLED_SCREEN_PLAYER_SETTINGS,
        .type = OLED_OPTION_TYPE_OPT,
        .name = oled_option_name_lock,
        .option_list = confirm_options,
        .option_list_len = confirm_options_len,
        .on_confirm_event = OLED_OPTION_EVENT_ASSIGN_LOCK,
    },

    // STICK
    { // LDEADZONE
        .screen = OLED_SCREEN_STICK_SETTINGS,
        .type = OLED_OPTION_TYPE_NUM,
        .step = 1,
        .min = 0,
        .max = 90,
        .def = 0,
        .name = oled_option_name_ldeadzone,
        .getter = get_ldeadzone,
        .setter = set_ldeadzone,
    },
    { // RDEADZONE
        .screen = OLED_SCREEN_STICK_SETTINGS,
        .type = OLED_OPTION_TYPE_NUM,
        .step = 1,
        .min = 0,
        .max = 90,
        .def = 0,
        .name = oled_option_name_rdeadzone,
        .getter = get_rdeadzone,
        .setter = set_rdeadzone,
    },
    { // LOUTMAX
        .screen = OLED_SCREEN_STICK_SETTINGS,
        .type = OLED_OPTION_TYPE_NUM,
        .step = 1,
        .min = 0,
        .max = 100,
        .def = 100,
        .name = oled_option_name_lmax,
        .getter = get_lmax,
        .setter = set_lmax,
    },
    { // ROUTMAX
        .screen = OLED_SCREEN_STICK_SETTINGS,
        .type = OLED_OPTION_TYPE_NUM,
        .step = 1,
        .min = 0,
        .max = 100,
        .def = 100,
        .name = oled_option_name_rmax,
        .getter = get_rmax,
        .setter = set_rmax,
    },
};

const size_t oled_screen_options_count = sizeof(oled_screen_options) / sizeof(oled_screen_options[0]);

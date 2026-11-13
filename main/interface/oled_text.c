#include "oled_display.h"

const char *oled_type_names[BT_TYPE_MAX] = {
    "Generic",
    "PS3",
    "Wii",
    "DualShock 4",
    "Switch",
    "Switch 2",
};

const char *oled_subtype_names[BT_SUBTYPE_MAX] = {
    "Default",
    "Nunchuk",
    "Wii Classic",
    "Wii 8Bit",
    "Wii Pro",
    "Wii Pro 8Bit",
    "Wii U Pro",
    "DualSense",
    "Joy-Con L",
    "Joy-Con R",
    "NES Pad",
    "SNES Pad",
    "N64 Pad",
    "Mega Drive",
    "Hyperkin",
};

const char *oled_name_unk = "Unknown";

#if OLED_LANG == EN
const char *oled_screen_names[OLED_SCREEN_MAX] = {
    [OLED_SCREEN_HOME] = "Home",
    [OLED_SCREEN_SETTINGS] = "Settings",
    [OLED_SCREEN_DIAGNOSTICS] = "Diagnostics",
    [OLED_SCREEN_PLAYER_SETTINGS] = "Player",
    [OLED_SCREEN_BUTTON_SETTINGS] = "Button",
    [OLED_SCREEN_STICK_SETTINGS] = "Stick",
    [OLED_SCREEN_VIBRATION_SETTINGS] = "Vibration Settings",

    [OLED_SCREEN_RECONNECT] = "Reconnect",
    [OLED_SCREEN_DISCONNECT] = "Disconnect",
    [OLED_SCREEN_ASSIGN_PLAYER] = "Assign Player",
    [OLED_SCREEN_AUTO_ASSIGN_PLAYER] = "Auto Assign",
};
#elif OLED_LANG == PTBR

#endif
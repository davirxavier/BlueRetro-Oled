//
// Created by fdavi on 13/11/2026.
//

#ifndef BLUE_RETRO_DUAL_PIO_OLED_OPTION_TEXT_H
#define BLUE_RETRO_DUAL_PIO_OLED_OPTION_TEXT_H

#if OLED_LANG == 0
static const char oled_option_name_diagnostics[] = "Diagnostics";
static const char oled_option_name_player[] = "Player";
static const char oled_option_name_button[] = "Button";
static const char oled_option_name_stick[] = "Stick";
static const char oled_option_name_vibration[] = "Vibration";

static const char oled_option_name_button_test[] = "Button Test";
static const char oled_option_name_stick_test[] = "Stick Test";
static const char oled_option_name_vibration_test[] = "Rumble Test";
static const char oled_option_name_low_battery[] = "LwBat Test";

static const char oled_option_name_reconnect[] = "Reconnect";
static const char oled_option_name_disconnect[] = "Disconnect";
static const char oled_option_name_assign_player[] = "Player";

static const char oled_option_name_ldeadzone[] = "LDeadzone";
static const char oled_option_name_rdeadzone[] = "RDeadzone";
static const char oled_option_name_lmax[] = "L Max";
static const char oled_option_name_rmax[] = "R Max";
#elif OLED_LANG == 1
static const char oled_option_name_diagnostics[] = "Dignosticos";
static const char oled_option_name_player[] = "Jogador";
static const char oled_option_name_button[] = "Botoes";
static const char oled_option_name_stick[] = "Joystick";
static const char oled_option_name_vibration[] = "Vibracao";

static const char oled_option_name_button_test[] = "Teste Botoes";
static const char oled_option_name_stick_test[] = "Teste Joystick";
static const char oled_option_name_vibration_test[] = "Teste Rumble";
static const char oled_option_name_low_battery[] = "Teste BatBaix";

static const char oled_option_name_reconnect[] = "Reconectar";
static const char oled_option_name_disconnect[] = "Desconectar";
static const char oled_option_name_assign_player[] = "Jogador";

static const char oled_option_name_ldeadzone[] = "Deadzone L";
static const char oled_option_name_rdeadzone[] = "Deadzone R";
static const char oled_option_name_lmax[] = "L Max";
static const char oled_option_name_rmax[] = "R Max";
#endif
#endif //BLUE_RETRO_DUAL_PIO_OLED_OPTION_TEXT_H

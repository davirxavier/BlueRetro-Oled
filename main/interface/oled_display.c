#include "oled_display.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "ssd1306.h"
#include "bluetooth/host.h"
#include "managed_components/chill-sam__ssd1306/private_include/ssd1306_private.h"

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

static ssd1306_handle_t display;
static oled_status_t status;
static uint32_t last_hash = 0;
static uint8_t oled_selected_index = 0;

enum
{
    OLED_SCREEN_HOME = 0,
    OLED_SCREEN_TEST,
    OLED_SCREEN_MAX,
};
static uint8_t current_screen = OLED_SCREEN_HOME;

static esp_err_t oled_draw_round_rect(ssd1306_handle_t display, int x, int y, int w, int h, int r, bool fill)
{
    if (w <= 0 || h <= 0 || r < 0) return ESP_ERR_INVALID_ARG;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    if (r == 0) return ssd1306_draw_rect(display, x, y, w, h, fill);

    if (fill)
    {
        esp_err_t err = ssd1306_draw_rect(display, x + r, y, w - 2 * r, h, true);
        if (err != ESP_OK) return err;
        err = ssd1306_draw_rect(display, x, y + r, w, h - 2 * r, true);
        if (err != ESP_OK) return err;
        if ((err = ssd1306_draw_circle(display, x + r, y + r, r, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_circle(display, x + w - r - 1, y + r, r, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_circle(display, x + r, y + h - r - 1, r, true)) != ESP_OK) return err;
        return ssd1306_draw_circle(display, x + w - r - 1, y + h - r - 1, r, true);
    }

    esp_err_t err;
    if ((err = ssd1306_draw_line(display, x + r, y, x + w - r - 1, y, true)) != ESP_OK) return err;
    if ((err = ssd1306_draw_line(display, x + r, y + h - 1, x + w - r - 1, y + h - 1, true)) != ESP_OK) return err;
    if ((err = ssd1306_draw_line(display, x, y + r, x, y + h - r - 1, true)) != ESP_OK) return err;
    if ((err = ssd1306_draw_line(display, x + w - 1, y + r, x + w - 1, y + h - r - 1, true)) != ESP_OK) return err;

    if ((err = ssd1306_draw_circle(display, x + r, y + r, r, false)) != ESP_OK) return err;
    if ((err = ssd1306_draw_circle(display, x + w - r - 1, y + r, r, false)) != ESP_OK) return err;
    if ((err = ssd1306_draw_circle(display, x + r, y + h - r - 1, r, false)) != ESP_OK) return err;
    return ssd1306_draw_circle(display, x + w - r - 1, y + h - r - 1, r, false);
}

static uint32_t fnv1a_hash(const void *data, size_t len)
{
    const uint8_t *bytes = data;
    uint32_t hash = 2166136261u;

    for (size_t i = 0; i < len; i++)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

static int oled_get_next_player(void)
{
    for (uint8_t out_idx = 0; out_idx < OLED_MAX_PLAYERS; out_idx++)
    {
        struct bt_dev *device = NULL;
        if (bt_host_get_active_dev_from_out_idx(out_idx, &device) < 0)
        {
            return out_idx;
        }
    }

    return -1;
}

static void oled_get_status(oled_status_t *status)
{
    memset(status, 0, sizeof(*status));
    status->pairing = bt_host_is_inquiry_active();
    status->pairingIndex = oled_get_next_player();

    for (int i = 0; i < OLED_MAX_PLAYERS; i++)
    {
        struct bt_ids *ids = bt_adapter.data[i].base.pids;
        oled_player_t *p = &status->players[i];
        p->connected = false;
        p->bt_data_base = &bt_adapter.data[i].base;
        p->name = oled_name_unk;

        if (ids && ids->type != BT_NONE)
        {
            p->connected = true;
            p->player = (uint8_t) ids->out_idx;

            if (ids->subtype == BT_SUBTYPE_DEFAULT || (ids->type > BT_NONE && ids->type < BT_TYPE_MAX))
            {
                p->name = oled_type_names[ids->type];
            }
            else if (ids->subtype > BT_SUBTYPE_DEFAULT && ids->subtype < BT_SUBTYPE_MAX)
            {
                p->name = oled_subtype_names[ids->subtype];
            }
        }
    }
}

static const oled_player_t *get_i_player(oled_status_t *status, uint8_t index)
{
    for (int i = 0; i < OLED_MAX_PLAYERS; i++)
    {
        const oled_player_t *p = &status->players[i];
        if (p->player == index)
        {
            return p;
        }
    }
    return NULL;
}

static uint8_t oled_get_text_x_right(ssd1306_handle_t display, const char *text)
{
    size_t size = strlen(text);
    if (text == NULL || size == 0)
    {
        return 0;
    }

    return display->width - size * (display->font->width + 1) - 4;
}

static void oled_draw_p_home(oled_status_t *status, uint8_t index, ssd1306_handle_t display, int lineStartY)
{
    const oled_player_t *p = get_i_player(status, index);
    char buf[32];
    int startLineX = 4;
    int startLineY = lineStartY + ((display->font->height + 5) * index);
    bool selected = (index == oled_selected_index);

    if (selected) oled_draw_round_rect(display, 1, startLineY - 3, display->width - 2, display->font->height + 6, 3, false);

    // Inverted player label
    snprintf(buf, sizeof(buf), "P%d", index + 1);
    int labelWidth = strlen(buf) * (display->font->width + 1) + 1;
    int labelHeight = display->font->height + 2;
    oled_draw_round_rect(display, startLineX, startLineY - 1, labelWidth, labelHeight, 2, true);
    ssd1306_draw_text(display, startLineX + 1, startLineY, buf, false);

    // Controller name
    snprintf(buf, sizeof(buf), "%s", p != NULL && p->name != NULL && p->connected ? p->name : "--");
    int name_x = startLineX + labelWidth + 4;
    ssd1306_draw_text(display, name_x, startLineY, buf, true);

    // Battery or connection status
    if (status->pairing && status->pairingIndex >= 0 && index == status->pairingIndex)
    {
        strcpy(buf, "PAIRING");
    }
    else if (p != NULL && p->connected && p->bt_data_base != NULL && p->bt_data_base->battery_valid)
    {
        snprintf(buf, sizeof(buf), "%u%%", (unsigned int)p->bt_data_base->battery_percent);
    }
    else if (index == status->pairingIndex)
    {
        strcpy(buf, "NEXT");
    }
    else if (p != NULL && p->connected)
    {
        strcpy(buf, "ON");
    }
    else {
        strcpy(buf, "--");
    }

    ssd1306_draw_text(display, oled_get_text_x_right(display, buf) - 6, startLineY, buf, true);
}

static void oled_draw_content(oled_status_t *status, ssd1306_handle_t display)
{
    ssd1306_clear(display);

    const int horizontalPadding = 4;
    const int verticalPadding = 2;
    const int headerStartY = 2;

    ssd1306_draw_text(display, horizontalPadding, headerStartY, "BLUE_RETR0", true);
    const char *bt_status = status->pairing ? "PAIRING" : "READY";
    ssd1306_draw_text(display, oled_get_text_x_right(display, bt_status), headerStartY, bt_status, true);

    if (current_screen == OLED_SCREEN_HOME)
    {
        int pStartY = headerStartY + display->font->height + (verticalPadding*3);
        for (uint8_t i = 0; i < OLED_MAX_PLAYERS; i++)
        {
            oled_draw_p_home(status, i, display, pStartY);
        }
    }
    else if (current_screen == OLED_SCREEN_TEST)
    {
        // TODO
    }

    ESP_ERROR_CHECK(ssd1306_display(display));
}

static void oled_update(bool force)
{
    oled_get_status(&status);

    uint32_t current_hash = fnv1a_hash(&status, sizeof(status));
    if (force || current_hash != last_hash)
    {
        last_hash = current_hash;
        oled_draw_content(&status, display);
    }
}

void oled_task(void *arg)
{
    // Initialize I2C
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_17,
        .scl_io_num = GPIO_NUM_16,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    // Configure OLED
    ssd1306_config_t cfg = {
        .bus = SSD1306_I2C,
        .width = 128,
        .height = 64,
        .iface.i2c = {
            .port = I2C_NUM_0,
            .addr = 0x3C,
            .rst_gpio = GPIO_NUM_17,
        },
    };

    ESP_ERROR_CHECK(ssd1306_new_i2c(&cfg, &display));
    while (1)
    {
        oled_update(false);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void oled_cmd(uint8_t cmd)
{
    switch (cmd)
    {
        case OLED_UP:
        {
            if (oled_selected_index > 0)
            {
                oled_selected_index--;
                buzzer_play(BUZZER_SONG_BUTTON);
            }
            else
            {
                buzzer_play(BUZZER_SONG_DISCONNECTED);
            }
            break;
        }
        case OLED_DOWN:
        {
            if (oled_selected_index < OLED_MAX_PLAYERS-1)
            {
                oled_selected_index++;
                buzzer_play(BUZZER_SONG_BUTTON);
            }
            else
            {
                buzzer_play(BUZZER_SONG_DISCONNECTED);
            }
            break;
        }
        case OLED_CONFIRM:
        {
            if (current_screen == OLED_SCREEN_HOME)
            {
                current_screen = OLED_SCREEN_TEST;
                buzzer_play(BUZZER_SONG_CONFIRM);
            }
            break;
        }
        case OLED_BACK:
        {
            if (current_screen == OLED_SCREEN_TEST)
            {
                current_screen = OLED_SCREEN_HOME;
                buzzer_play(BUZZER_SONG_CONFIRM);
            }
            else
            {
                buzzer_play(BUZZER_SONG_DISCONNECTED);
            }
            break;
        }
        default: ;
    }

    oled_update(true);
}


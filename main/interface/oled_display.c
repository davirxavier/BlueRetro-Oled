#include "oled_display.h"

#include <esp_log.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "ssd1306.h"
#include "bluetooth/host.h"
#include "managed_components/chill-sam__ssd1306/private_include/ssd1306_private.h"

#define OLED_CURR_OPTIONS_LEN 3
#define OLED_NAV_STACK_SIZE 4

static QueueHandle_t event_queue = NULL;
static ssd1306_handle_t display;
static oled_status_t status;
static uint8_t oled_selected_index = 0;
static bool is_choosing = false;
static uint8_t started_choosing_value = 0;
uint8_t oled_current_slot = 0;

typedef struct {
    bool visible;
    bool selected;
    const char *title;
    const char *message;
    void (*on_confirm)(bool result);
} oled_dialog_t;

static oled_dialog_t dialog = {0};

static uint8_t current_screen = OLED_SCREEN_HOME;
static uint8_t navigation_stack[OLED_NAV_STACK_SIZE];
static uint8_t navigation_depth = 0;

static void oled_navigate_to(uint8_t screen)
{
    if (screen >= OLED_SCREEN_MAX) return;
    if (navigation_depth >= OLED_NAV_STACK_SIZE) return;

    navigation_stack[navigation_depth++] = current_screen;
    current_screen = screen;
    oled_selected_index = 0;
}

static void oled_navigate_back(void)
{
    if (navigation_depth == 0) return;

    current_screen = navigation_stack[--navigation_depth];
    oled_selected_index = 0;

    if (current_screen == OLED_SCREEN_HOME)
    {
        oled_current_slot = 0;
    }
}

static esp_err_t oled_draw_round_rect(ssd1306_handle_t display, int x, int y, int w, int h, int r, bool fill)
{
    if (w <= 0 || h <= 0 || r < 0) return ESP_ERR_INVALID_ARG;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    if (r == 0) return ssd1306_draw_rect(display, x, y, w, h, fill);

    esp_err_t err;

    if (fill)
    {
        // Central rectangle
        if ((err = ssd1306_draw_rect(display, x + r, y, w - 2 * r, h, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_rect(display, x, y + r, w, h - 2 * r, true)) != ESP_OK) return err;

        // Rounded corners using midpoint-circle algorithm
        int dx = r;
        int dy = 0;
        int d = 1 - r;

        while (dx >= dy)
        {
            // Top scanlines
            if ((err = ssd1306_draw_line(display, x + r - dx, y + r - dy, x + w - r - 1 + dx, y + r - dy, true)) != ESP_OK) return err;
            if (dy != 0 && (err = ssd1306_draw_line(display, x + r - dy, y + r - dx, x + w - r - 1 + dy, y + r - dx, true)) != ESP_OK) return err;

            // Bottom scanlines
            if ((err = ssd1306_draw_line(display, x + r - dx, y + h - r - 1 + dy, x + w - r - 1 + dx, y + h - r - 1 + dy, true)) != ESP_OK) return err;
            if (dy != 0 && (err = ssd1306_draw_line(display, x + r - dy, y + h - r - 1 + dx, x + w - r - 1 + dy, y + h - r - 1 + dx, true)) != ESP_OK) return err;

            if (d < 0)
                d += 2 * dy + 3;
            else
            {
                d += 2 * (dy - dx) + 5;
                dx--;
            }
            dy++;
        }

        return ESP_OK;
    }

    // Straight edges
    if ((err = ssd1306_draw_line(display, x + r, y, x + w - r - 1, y, true)) != ESP_OK) return err;
    if ((err = ssd1306_draw_line(display, x + r, y + h - 1, x + w - r - 1, y + h - 1, true)) != ESP_OK) return err;
    if ((err = ssd1306_draw_line(display, x, y + r, x, y + h - r - 1, true)) != ESP_OK) return err;
    if ((err = ssd1306_draw_line(display, x + w - 1, y + r, x + w - 1, y + h - r - 1, true)) != ESP_OK) return err;

    // Rounded corners using individual pixels
    int dx = r;
    int dy = 0;
    int d = 1 - r;

    while (dx >= dy)
    {
        // Top-left
        if ((err = ssd1306_draw_pixel(display, x + r - dx, y + r - dy, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_pixel(display, x + r - dy, y + r - dx, true)) != ESP_OK) return err;

        // Top-right
        if ((err = ssd1306_draw_pixel(display, x + w - r - 1 + dx, y + r - dy, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_pixel(display, x + w - r - 1 + dy, y + r - dx, true)) != ESP_OK) return err;

        // Bottom-left
        if ((err = ssd1306_draw_pixel(display, x + r - dx, y + h - r - 1 + dy, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_pixel(display, x + r - dy, y + h - r - 1 + dx, true)) != ESP_OK) return err;

        // Bottom-right
        if ((err = ssd1306_draw_pixel(display, x + w - r - 1 + dx, y + h - r - 1 + dy, true)) != ESP_OK) return err;
        if ((err = ssd1306_draw_pixel(display, x + w - r - 1 + dy, y + h - r - 1 + dx, true)) != ESP_OK) return err;

        if (d < 0)
            d += 2 * dy + 3;
        else
        {
            d += 2 * (dy - dx) + 5;
            dx--;
        }
        dy++;
    }

    return ESP_OK;
}

static void oled_dialog_show(const char *title, const char *message, void (*callback)(bool))
{
    dialog.visible = true;
    dialog.selected = false;
    dialog.title = title;
    dialog.message = message;
    dialog.on_confirm = callback;
}

static void oled_dialog_close(void)
{
    dialog.visible = false;
    dialog.on_confirm = NULL;
}

static void oled_dialog_draw(void)
{
    if (!dialog.visible) return;

    int w = 112;
    int h = 42;
    int x = (display->width - w) / 2;
    int y = (display->height - h) / 2;

    // Dialog background and border
    oled_draw_round_rect(display, x, y, w, h, 4, true);
    oled_draw_round_rect(display, x + 1, y + 1, w - 2, h - 2, 3, false);

    // Title
    ssd1306_draw_text(display, x + 5, y + 4, dialog.title, false);

    // Message
    ssd1306_draw_text(display, x + 5, y + 16, dialog.message, false);

    // Options
    ssd1306_draw_text(display, x + 22, y + 29, "YES", !dialog.selected);
    ssd1306_draw_text(display, x + 70, y + 29, "NO", dialog.selected);
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

static const oled_player_t *get_i_player(uint8_t index)
{
    for (int i = 0; i < OLED_MAX_PLAYERS; i++)
    {
        const oled_player_t *p = &status.players[i];
        if (p->player == index)
        {
            return p;
        }
    }
    return NULL;
}

static uint8_t oled_get_text_x_right(ssd1306_handle_t display, const char *text)
{
    if (text == NULL)
    {
        return 0;
    }

    size_t size = strlen(text);
    if (size == 0)
    {
        return 0;
    }

    return display->width - size * (display->font->width + 1) - 4;
}

static void oled_draw_p_home(uint8_t index, int lineStartY)
{
    const oled_player_t *p = get_i_player(index);
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
    if (status.pairing && status.pairingIndex >= 0 && index == status.pairingIndex)
    {
        strcpy(buf, "PAIRING");
    }
    else if (p != NULL && p->connected && p->bt_data_base != NULL && p->bt_data_base->battery_valid)
    {
        snprintf(buf, sizeof(buf), "%u%%", (unsigned int)p->bt_data_base->battery_percent);
    }
    else if (index == status.pairingIndex)
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

static size_t get_options_count(uint8_t screen)
{
    size_t options_count = 0;

    for (size_t i = 0; i < oled_screen_options_count; i++)
    {
        if (screen == oled_screen_options[i].screen)
        {
            options_count++;
        }
    }

    return options_count;
}

static oled_screen_option_t *get_selected_option()
{
    oled_screen_option_t *selected_option = NULL;
    size_t matching_index = 0;

    for (size_t i = 0; i < oled_screen_options_count; i++)
    {
        if (current_screen != oled_screen_options[i].screen)
        {
            continue;
        }

        if (matching_index == oled_selected_index)
        {
            selected_option = &oled_screen_options[i];
            break;
        }

        matching_index++;
    }

    return selected_option;
}

static void oled_draw_scroll_screen(uint8_t lineStartY)
{
    size_t options_count = get_options_count(current_screen);
    if (oled_selected_index >= options_count)
    {
        oled_selected_index = options_count - 1;
    }

    size_t first_visible = oled_selected_index >= OLED_CURR_OPTIONS_LEN ? oled_selected_index - OLED_CURR_OPTIONS_LEN + 1 : 0;
    const oled_screen_option_t *visible_options[OLED_CURR_OPTIONS_LEN] = {0};
    size_t matching_index = 0;
    for (size_t i = 0; i < oled_screen_options_count; i++)
    {
        if (current_screen != oled_screen_options[i].screen)
            continue;

        if (matching_index >= first_visible && matching_index < first_visible + OLED_CURR_OPTIONS_LEN)
        {
            visible_options[matching_index - first_visible] = &oled_screen_options[i];
        }

        matching_index++;
    }

    int startLineX = 5;
    for (uint8_t i = 0; i < OLED_CURR_OPTIONS_LEN; i++)
    {
        const oled_screen_option_t *option = visible_options[i];
        if (option == NULL || option->name == NULL) continue;

        int startY = lineStartY + ((display->font->height + 6) * i) + 4;
        bool selected = (first_visible + i) == oled_selected_index;

        if (selected && !is_choosing)
        {
            oled_draw_round_rect(display, 1, startY - 3, display->width - 8, display->font->height + 6, 3, false);
        }

        ssd1306_draw_text(display, startLineX, startY, option->name, true);

        int valueX = 0;
        int valueWidth = 0;
        char valueBuf[10] = {0};

        uint8_t optionValue = option->def;
        if (option->getter != NULL)
        {
            optionValue = option->getter();
        }

        switch (option->type)
        {
            case OLED_OPTION_TYPE_NUM:
                snprintf(valueBuf, sizeof(valueBuf), "%d%s", optionValue, option->suffix);
                break;

            case OLED_OPTION_TYPE_OPT:
                if (optionValue < option->option_list_len)
                {
                    snprintf(valueBuf, sizeof(valueBuf), "%s", option->option_list[optionValue]);
                }
                break;

            default:
                snprintf(valueBuf, sizeof(valueBuf), ">");
                break;
        }

        valueWidth = strlen(valueBuf) * (display->font->width + 1);
        valueX = display->width - valueWidth - 10;

        if (is_choosing && selected)
        {
            oled_draw_round_rect(display, valueX - 3, startY - 3, valueWidth + 6, display->font->height + 6, 2, true);
            ssd1306_draw_text(display, valueX, startY, valueBuf, false);
        }
        else
        {
            ssd1306_draw_text(display, valueX, startY, valueBuf, true);
        }
    }

    int scrollbarX = display->width - 3;
    int scrollbarY = lineStartY + 1;
    int scrollbarHeight = (display->font->height + 6) * OLED_CURR_OPTIONS_LEN;

    ssd1306_draw_line(display, scrollbarX, scrollbarY, scrollbarX, scrollbarY + scrollbarHeight, true);

    int thumbHeight = options_count > OLED_CURR_OPTIONS_LEN ? scrollbarHeight * OLED_CURR_OPTIONS_LEN / options_count : scrollbarHeight;
    if (thumbHeight < 4) thumbHeight = 4;

    int maxScroll = options_count > OLED_CURR_OPTIONS_LEN ? options_count - OLED_CURR_OPTIONS_LEN : 0;
    int scrollIndex = first_visible;
    int thumbY = scrollbarY;

    if (maxScroll > 0)
    {
        thumbY += (scrollbarHeight - thumbHeight) * scrollIndex / maxScroll;
    }

    oled_draw_round_rect(display, scrollbarX - 1, thumbY, 3, thumbHeight, 1, true);
}

static void oled_draw_content()
{
    ssd1306_clear(display);

    if (dialog.visible)
    {
        oled_dialog_draw();
        ESP_ERROR_CHECK(ssd1306_display(display));
        return;
    }

    const int horizontalPadding = 4;
    const int verticalPadding = 2;
    const int headerStartY = 2;
    int pStartY = headerStartY + display->font->height + (verticalPadding*3);

    if (current_screen == OLED_SCREEN_HOME)
    {
        ssd1306_draw_text(display, horizontalPadding, headerStartY, "BLUE_RETR0", true);
        const char *bt_status = status.pairing ? "PAIRING" : "READY";
        ssd1306_draw_text(display, oled_get_text_x_right(display, bt_status), headerStartY, bt_status, true);

        for (uint8_t i = 0; i < OLED_MAX_PLAYERS; i++)
        {
            oled_draw_p_home(i, pStartY);
        }
    }
    else if (current_screen >= OLED_SCREEN_SETTINGS)
    {
        ssd1306_draw_text(display, horizontalPadding, headerStartY, oled_screen_names[current_screen], true);
        int lineY = headerStartY + display->font->height + 2;
        ssd1306_draw_line(display, 0, lineY, display->width-1, lineY, true);

        char name[32];
        snprintf(name, sizeof(name), "SLOT%d", oled_current_slot+1);
        ssd1306_draw_text(display, oled_get_text_x_right(display, name), headerStartY, name, true);

        oled_draw_scroll_screen(pStartY);
    }

    ESP_ERROR_CHECK(ssd1306_display(display));
}

static void oled_update()
{
    oled_get_status(&status);
    oled_draw_content();
}

static void oled_cmd_process(oled_cmd_t cmd);

void oled_config_save(void)
{
    config_update(config_get_src());
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
            .rst_gpio = GPIO_NUM_NC,
        },
    };

    ESP_ERROR_CHECK(ssd1306_new_i2c(&cfg, &display));
    event_queue = xQueueCreate(10, sizeof(oled_cmd_t));
    if (event_queue == NULL)
    {
        ESP_LOGE("OLED", "Event queue creation failed");
    }

    oled_cmd_t cmd;
    while (1)
    {
        if (xQueueReceive(event_queue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            oled_cmd_process(cmd);
        }

        oled_update();
    }
}

void oled_update_vals()
{
 // TODO load
}

static bool oled_dialog_cmd(oled_cmd_t cmd)
{
    if (!dialog.visible) return false;

    switch (cmd)
    {
        case OLED_UP:
        case OLED_DOWN:
            dialog.selected = !dialog.selected;
            buzzer_play(BUZZER_SONG_CHANGE);
            break;

        case OLED_CONFIRM:
        {
            bool result = !dialog.selected;
            void (*callback)(bool) = dialog.on_confirm;

            oled_dialog_close();

            if (callback != NULL)
                callback(result);

            buzzer_play(BUZZER_SONG_CHANGE);
            break;
        }

        case OLED_BACK:
            oled_dialog_close();
            buzzer_play(BUZZER_SONG_DISCONNECTED);
            break;

        default:
            break;
    }

    return true;
}

static void do_reconnect(bool acc)
{

}

static void handle_option(const oled_screen_option_t *option)
{
    if (option == NULL)
    {
        return;
    }

    if (is_choosing)
    {
        is_choosing = false;
        oled_config_save();
        started_choosing_value = 0;
    }
    else if (option->type == OLED_OPTION_TYPE_NUM || option->type == OLED_OPTION_TYPE_OPT)
    {
        is_choosing = true;
        started_choosing_value = option->getter != NULL ? option->getter() : option->def;
        return;
    }

    // TODO

    switch (option->on_confirm_event)
    {
        case OLED_OPTION_EVENT_RECONNECT:
        {
            oled_dialog_show("Confirm reconnect?", "", do_reconnect);
            break;
        }
        default: ;
    }
}

static void handle_choosing_move(bool up)
{
    oled_screen_option_t *option = get_selected_option();
    if (option == NULL || option->type == OLED_OPTION_NONE)
    {
        return;
    }

    if (option->type == OLED_OPTION_TYPE_NUM && option->setter != NULL)
    {
        int val = option->getter != NULL ? option->getter() : option->def;
        val += up ? option->step : -option->step;

        if (val < option->min)
        {
            val = option->min;
        }
        else if (val > option->max)
        {
            val = option->max;
        }

        option->setter(val);
    }
}

static void oled_cmd_process(oled_cmd_t cmd)
{
    if (cmd == OLED_REFRESH || oled_dialog_cmd(cmd))
    {
        return;
    }

    bool isHome = current_screen == OLED_SCREEN_HOME;
    switch (cmd)
    {
        case OLED_UP:
        {
            if (is_choosing)
            {
                handle_choosing_move(true);
                buzzer_play(BUZZER_SONG_CHANGE);
                break;
            }

            if (oled_selected_index > 0)
            {
                oled_selected_index--;
                buzzer_play(BUZZER_SONG_CHANGE);
            }
            else
            {
                buzzer_play(BUZZER_SONG_DISCONNECTED);
            }
            break;
        }
        case OLED_DOWN:
        {
            if (is_choosing)
            {
                handle_choosing_move(false);
                buzzer_play(BUZZER_SONG_CHANGE);
                break;
            }

            size_t options_count = isHome ? OLED_MAX_PLAYERS : get_options_count(current_screen);

            if (options_count > 0 && oled_selected_index < options_count - 1)
            {
                oled_selected_index++;
                buzzer_play(BUZZER_SONG_CHANGE);
            }
            else
            {
                buzzer_play(BUZZER_SONG_DISCONNECTED);
            }
            break;
        }
        case OLED_CONFIRM:
        {
            const oled_screen_option_t *option = get_selected_option();
            if (!isHome && option == NULL)
            {
                break;
            }

            if (isHome || option->type == OLED_OPTION_TYPE_NAV)
            {
                int new_screen = -1;

                if (current_screen == OLED_SCREEN_HOME)
                {
                    oled_current_slot = oled_selected_index;
                    new_screen = OLED_SCREEN_SETTINGS;
                }
                else
                {
                    new_screen = option->navigation_screen;
                }

                if (new_screen < 0 || new_screen >= OLED_SCREEN_MAX)
                {
                    buzzer_play(BUZZER_SONG_DISCONNECTED);
                    break;
                }

                oled_navigate_to(new_screen);
                buzzer_play(BUZZER_SONG_CHANGE);
            }
            else
            {
                handle_option(option);
                buzzer_play(BUZZER_SONG_CHANGE);
            }
            break;
        }
        case OLED_BACK:
        {
            if (is_choosing)
            {
                const oled_screen_option_t *option = get_selected_option();
                if (option->setter != NULL)
                {
                    option->setter(started_choosing_value);
                }
                buzzer_play(BUZZER_SONG_ERROR);
                is_choosing = false;
                break;
            }

            if (navigation_depth == 0)
            {
                buzzer_play(BUZZER_SONG_DISCONNECTED);
            }
            else
            {
                buzzer_play(BUZZER_SONG_CHANGE);
            }

            oled_navigate_back();
            break;
        }
        default: ;
    }
}

void oled_cmd(oled_cmd_t cmd)
{
    if (event_queue == NULL) return;

    if (xQueueSend(event_queue, &cmd, 0) != pdTRUE)
    {
        ESP_LOGW("OLED", "Event queue full");
    }
}

#include "sw_runtime.h"
#include "muse_board.h"
#include "muse_chat.h"
#include "muse_link.h"
#include "muse_settings.h"
#include "muse_state.h"
#include "muse_wifi.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static atomic_uint next_page;
#if LV_USE_SNAPSHOT
static atomic_uint documentation_page;
void sw_pages_documentation(unsigned page) { atomic_store(&documentation_page, page); }
#endif
static lv_obj_t *tiles[4], *setup_info, *status_text, *history_text, *pair_button;
static lv_font_t text_font;
static EXT_RAM_BSS_ATTR sw_history_t snapshot;
static unsigned seen_version = UINT32_MAX;
static int64_t next_tick;
static unsigned page_index;
LV_FONT_DECLARE(muse_font_cjk_16);

void sw_pages_next(void) { atomic_fetch_add(&next_page, 1); }

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_width(o, 320);
    lv_obj_set_style_text_font(o, &text_font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0xe8edf8), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(o, text);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    return o;
}

static void setup(lv_event_t *e) { (void)e; sw_portal_request(); muse_state_poke(); }
static void pair(lv_event_t *e) { (void)e; sw_pairing_request(); muse_state_poke(); }
static void mute(lv_event_t *e) { (void)e; muse_settings_set_speaker_on(!muse_settings_speaker_on()); muse_state_poke(); }
static lv_obj_t *button(lv_obj_t *parent, const char *text, int y, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 270, 44); lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x23473f), 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, text); lv_obj_center(l);
    return b;
}

void sw_pages_build(lv_obj_t *tv, lv_obj_t *settings)
{
    text_font = lv_font_montserrat_16;
    text_font.fallback = &muse_font_cjk_16;
    tiles[0] = lv_tileview_get_tile_active(tv);
    tiles[1] = settings;
    tiles[2] = lv_tileview_add_tile(tv, 2, 0, LV_DIR_HOR);
    tiles[3] = lv_tileview_add_tile(tv, 3, 0, LV_DIR_LEFT);
    for (int i = 1; i < 4; ++i) {
        lv_obj_set_style_bg_color(tiles[i], lv_color_hex(0x0a101b), 0);
        lv_obj_set_style_bg_opa(tiles[i], LV_OPA_COVER, 0);
        lv_obj_set_scrollbar_mode(tiles[i], LV_SCROLLBAR_MODE_OFF);
    }
    label(settings, "SETTINGS", 52);
    button(settings, "Open Wi-Fi setup", 96, setup);
    button(settings, "Toggle voice playback", 148, mute);
    pair_button = button(settings, "Restart Muse pairing", 200, pair);
    setup_info = label(settings, "", 258);
    label(tiles[2], "DEVICE STATUS", 52);
    status_text = label(tiles[2], "Connecting...", 110);
    label(tiles[3], "RECENT CONVERSATION", 52);
    lv_obj_t *scroll = lv_obj_create(tiles[3]);
    lv_obj_set_size(scroll, 340, 270); lv_obj_align(scroll, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(scroll, 0, 0);
    history_text = label(scroll, "No messages yet", 0);
    lv_obj_set_style_text_align(history_text, LV_TEXT_ALIGN_LEFT, 0);
}

void sw_pages_tick(lv_obj_t *tv)
{
#if LV_USE_SNAPSHOT
    unsigned demo = atomic_load(&documentation_page);
    if (demo >= 1 && demo <= 3) {
        lv_tileview_set_tile(tv, tiles[demo], LV_ANIM_OFF);
        lv_label_set_text(setup_info, "Muse-Setup (sample)\nPassword: 12345678 (sample)\nhttp://192.168.4.1\nVoice: ON");
        lv_label_set_text(status_text, "Muse: online (sample)\nBLE: off after pairing\nSDK token: saved\nWi-Fi: connected\nRSSI: -48 dBm\nIP: 192.168.1.100\nBattery: 85%\nTTS: Ready\nFree RAM: 48 KB\nDocumentation demo");
        lv_label_set_text(history_text, "You: Help me plan tomorrow.\n\nMuse: What time would you like to start?\n\nSample conversation");
        return;
    }
#endif
    unsigned advance = atomic_exchange(&next_page, 0);
    lv_obj_t *active = lv_tileview_get_tile_active(tv);
    for (unsigned i = 0; i < 4; ++i) if (active == tiles[i]) page_index = i;
    if (advance) {
        page_index = (page_index + advance) % 4;
        lv_tileview_set_tile(tv, tiles[page_index], LV_ANIM_OFF);
    }
    if (esp_timer_get_time() < next_tick) return;
    next_tick = esp_timer_get_time() + 1000000;
    char name[33], password[17], tts[48]; bool on;
    sw_portal_info(name, sizeof(name), password, sizeof(password), &on);
    sw_tts_status(tts, sizeof(tts));
    if (muse_link_hatch_linked()) lv_obj_add_state(pair_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(pair_button, LV_STATE_DISABLED);
    if (page_index == 1) {
        if (on) lv_label_set_text_fmt(setup_info, "%s\nPassword: %s\nhttp://192.168.4.1\nVoice: %s", name, password,
                                     muse_settings_speaker_on() ? "ON" : "OFF");
        else lv_label_set_text_fmt(setup_info, "Setup hotspot is off\nVoice: %s\nWi-Fi / SDK token / Doubao\nare configured in your browser",
                                  muse_settings_speaker_on() ? "ON" : "OFF");
    }
    if (page_index == 2) {
        muse_wifi_status_t w; muse_wifi_status(&w);
        muse_hatch_status_t m; muse_hatch_status(&m);
        muse_power_t p = muse_state_power();
        char battery[20];
        if (p.battery_pct < 0) strcpy(battery, "unavailable");
        else snprintf(battery, sizeof(battery), "%d%%", p.battery_pct);
        lv_label_set_text_fmt(status_text,
            "Muse: %s\n%s\nBLE: %s\nSDK token: %s\nWi-Fi: %s\nRSSI: %d dBm\nIP: %s\nBattery: %s%s\nTTS: %s\nFree RAM: %u KB\nUptime: %llu min",
            muse_link_hatch_linked() ? muse_hatch_state_name(m.state) : "Not paired", m.detail,
            muse_link_hatch_linked() && !strcmp(sw_ble_state(),"idle") ? "off after pairing" : sw_ble_state(),
            muse_link_sdk_token() ? "saved" : "missing", w.state == MUSE_WIFI_CONNECTED ? "connected" : "offline",
            w.rssi, w.ip, battery, p.charging ? " charging" : "", tts,
            (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
            (unsigned long long)(esp_timer_get_time() / 60000000));
    }
    if (page_index == 3) {
        sw_history_snapshot(&snapshot);
        if (snapshot.version != seen_version) {
            /* Bounded to eight entries; copied once per history change. */
            static EXT_RAM_BSS_ATTR char text[SW_HISTORY_COUNT * (SW_TEXT_CAP + 16)];
            text[0] = 0; size_t used = 0;
            for (unsigned i = 0; i < snapshot.count; ++i) {
                const sw_entry_t *e = sw_history_get(&snapshot, i);
                int n = snprintf(text + used, sizeof(text) - used, "%s\n%s\n\n", e->assistant ? "MUSE" : "YOU", e->text);
                if (n < 0 || (size_t)n >= sizeof(text) - used) break;
                used += n;
            }
            lv_label_set_text(history_text, text[0] ? text : "No messages yet");
            seen_version = snapshot.version;
        }
    }
    memset(password, 0, sizeof(password));
}

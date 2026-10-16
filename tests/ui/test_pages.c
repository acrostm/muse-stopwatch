#include "sw_runtime.h"
#include "ui_stubs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void muse_state_poke(void) {}
bool muse_settings_speaker_on(void) { return true; }
void muse_settings_set_speaker_on(bool on) { (void)on; }
bool muse_link_hatch_linked(void) { return true; }
const char *muse_link_sdk_token(void) { return "fixture"; }
void muse_wifi_status(muse_wifi_status_t *out) { *out = (muse_wifi_status_t){.state=1}; }
void muse_hatch_status(muse_hatch_status_t *out) { *out = (muse_hatch_status_t){0}; }
muse_power_t muse_state_power(void) { return (muse_power_t){.battery_pct=85}; }
const char *muse_hatch_state_name(int state) { (void)state; return "Connected"; }
size_t heap_caps_get_free_size(int caps) { (void)caps; return 50000; }
int64_t esp_timer_get_time(void) { static int64_t time; return time += 1000000; }
void sw_portal_request(void) {}
void sw_pairing_request(void) {}
void sw_portal_info(char *ssid,size_t sc,char *pass,size_t pc,bool *on) {
    (void)sc;(void)pc;ssid[0]=pass[0]=0;*on=false;
}
void sw_tts_status(char *out,size_t cap) { sw_text_copy(out,cap,"Ready"); }
const char *sw_ble_state(void) { return "idle"; }
void sw_history_snapshot(sw_history_t *out) { *out = (sw_history_t){0}; }
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
    (void)area;(void)pixels;lv_display_flush_ready(display);
}
int main(void) {
    lv_init();
    lv_display_t *display = lv_display_create(466,466);
    static uint8_t pixels[466*16*2];
    lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_obj_t *tv = lv_tileview_create(lv_screen_active());
    lv_obj_t *face = lv_tileview_add_tile(tv,0,0,LV_DIR_RIGHT);
    lv_obj_t *settings = lv_tileview_add_tile(tv,1,0,LV_DIR_HOR);
    /* Real LVGL starts with NULL active tile, even after adding children. */
    assert(lv_tileview_get_tile_active(tv) == NULL);
    sw_pages_build(tv,face,settings);
    assert(lv_tileview_get_tile_active(tv) == face);
    assert(sw_pages_current() == 0);
    lv_obj_update_layout(tv);
    sw_pages_tick(tv);
    for (unsigned i=1; i<=20; ++i) {
        sw_pages_next(); sw_pages_tick(tv);
        assert(sw_pages_current() == i%4);
        assert(lv_tileview_get_tile_active(tv) == lv_obj_get_child(tv,i%4));
        lv_tick_inc(20); lv_timer_handler();
    }
    lv_tileview_set_tile(tv,lv_obj_get_child(tv,2),LV_ANIM_OFF);
    sw_pages_tick(tv); assert(sw_pages_current() == 2);
    for (unsigned i=0;i<100;++i) sw_pages_next();
    sw_pages_tick(tv); assert(sw_pages_current() == 2);
    lv_display_delete(display);lv_deinit();
    puts("real LVGL page initialization, rendering, swipe and cycle: PASS");
}

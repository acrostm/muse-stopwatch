#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define EXT_RAM_BSS_ATTR
#define MALLOC_CAP_INTERNAL 1
#define ESP_OK 0
typedef int esp_err_t;
typedef struct { int battery_pct; bool charging; } muse_power_t;
typedef struct { int state, rssi; char ip[16]; } muse_wifi_status_t;
typedef struct { int state; char detail[64]; } muse_hatch_status_t;
#define MUSE_WIFI_CONNECTED 1
void muse_state_poke(void);
bool muse_settings_speaker_on(void);
void muse_settings_set_speaker_on(bool on);
bool muse_link_hatch_linked(void);
const char *muse_link_sdk_token(void);
void muse_wifi_status(muse_wifi_status_t *out);
void muse_hatch_status(muse_hatch_status_t *out);
muse_power_t muse_state_power(void);
const char *muse_hatch_state_name(int state);
size_t heap_caps_get_free_size(int caps);
int64_t esp_timer_get_time(void);

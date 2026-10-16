#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct { char host[16]; uint16_t port; bool enabled; } sw_proxy_config_t;
esp_err_t sw_proxy_init(void);
void sw_proxy_get(sw_proxy_config_t *out);
esp_err_t sw_proxy_save(const sw_proxy_config_t *config);
bool sw_proxy_request(const char *host, uint16_t port);
void sw_proxy_tick(void);

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool sw_proxy_target(const char *host);
bool sw_proxy_lan_ipv4(const char *host);
size_t sw_socks_request(const char *host, uint16_t port, unsigned char *out, size_t cap);

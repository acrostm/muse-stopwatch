import os
"""Exercise real socket routing through a fragmented local SOCKS5 peer."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
STUB=r'''
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define EXT_RAM_BSS_ATTR
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_ARG 2
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define portMAX_DELAY 0
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
typedef int esp_err_t;
typedef int nvs_handle_t;
typedef pthread_mutex_t *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void){pthread_mutex_t *p=malloc(sizeof(*p));pthread_mutex_init(p,NULL);return p;}
static inline int xSemaphoreTake(SemaphoreHandle_t p,int wait){(void)wait;return pthread_mutex_lock(p);}
static inline void xSemaphoreGive(SemaphoreHandle_t p){pthread_mutex_unlock(p);}
static inline int64_t esp_timer_get_time(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static inline int nvs_open(const char *s,int mode,nvs_handle_t *h){(void)s;(void)mode;(void)h;return -1;}
static inline int nvs_set_blob(int h,const char *s,const void *p,size_t n){(void)h;(void)s;(void)p;(void)n;return 0;}
static inline int nvs_get_blob(int h,const char *s,void *p,size_t *n){(void)h;(void)s;(void)p;(void)n;return -1;}
static inline int nvs_commit(int h){(void)h;return 0;}
static inline void nvs_close(int h){(void)h;}
static inline void vTaskDelay(int t){(void)t;}
static inline void esp_restart(void){}
'''
class Proxy(unittest.TestCase):
    def test_real_socket_routing(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'proxy_stubs.h').write_text(STUB)
            for name in ['esp_err.h','esp_timer.h','esp_log.h','esp_system.h','esp_attr.h','freertos/FreeRTOS.h','freertos/semphr.h','freertos/task.h','lwip/netdb.h','lwip/sockets.h','nvs.h']:
                h=p/name;h.parent.mkdir(exist_ok=True,parents=True);h.write_text('#include "proxy_stubs.h"\n')
            binary=p/'proxy'
            subprocess.run([os.environ.get('CC', 'cc'),'-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-pthread','-I'+str(p),'-I'+str(ROOT/'src'),str(ROOT/'tests/test_proxy.c'),str(ROOT/'src/sw_proxy_core.c'),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True,timeout=20)

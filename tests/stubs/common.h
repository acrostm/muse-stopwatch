#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_FAIL -1
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define portMAX_DELAY 0xffffffff
#define pdTRUE 1
#define pdPASS 1
#define tskNO_AFFINITY -1
#define pdMS_TO_TICKS(n) (n)
typedef int esp_err_t;
typedef void *QueueHandle_t;
typedef void *SemaphoreHandle_t;
typedef struct { unsigned char bytes[65536]; size_t size; } fake_stream_t;
typedef fake_stream_t *StreamBufferHandle_t;
typedef struct lv_obj_t lv_obj_t;
typedef struct {
    const char *url; int method, timeout_ms;
    void *crt_bundle_attach; int buffer_size; bool disable_auto_redirect;
} esp_http_client_config_t;
typedef void *esp_http_client_handle_t;
#define HTTP_METHOD_POST 1
static inline void esp_crt_bundle_attach(void) {}
static int64_t clock_us;
static inline int64_t esp_timer_get_time(void) { return clock_us; }
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
static inline int xSemaphoreTake(SemaphoreHandle_t s, unsigned wait) { (void)s; (void)wait; return 1; }
static inline void xSemaphoreGive(SemaphoreHandle_t s) { (void)s; }
static inline QueueHandle_t xQueueCreate(unsigned a, unsigned b) { (void)a; (void)b; return (void *)1; }
static inline int xQueueOverwrite(QueueHandle_t q, const void *p) { (void)q; (void)p; return 1; }
static inline int xQueueReceive(QueueHandle_t q, void *p, unsigned wait) { (void)q; (void)p; (void)wait; return 0; }
static inline StreamBufferHandle_t xStreamBufferCreateWithCaps(size_t n, int trigger, int caps) { (void)n; (void)trigger; (void)caps; return calloc(1, sizeof(fake_stream_t)); }
static inline int xStreamBufferReset(StreamBufferHandle_t s) { s->size = 0; return 1; }
static inline int xStreamBufferIsEmpty(StreamBufferHandle_t s) { return !s->size; }
static inline size_t xStreamBufferBytesAvailable(StreamBufferHandle_t s) { return s->size; }
static inline size_t xStreamBufferSend(StreamBufferHandle_t s, const void *p, size_t n, unsigned wait) {
    (void)wait; if (n > sizeof(s->bytes)-s->size) n = sizeof(s->bytes)-s->size;
    memcpy(s->bytes+s->size,p,n); s->size+=n; return n;
}
static inline size_t xStreamBufferReceive(StreamBufferHandle_t s, void *p, size_t n, unsigned wait) {
    (void)wait; if(n>s->size)n=s->size; memcpy(p,s->bytes,n);memmove(s->bytes,s->bytes+n,s->size-n);s->size-=n;return n;
}
static inline int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned pri, void *handle) {
    (void)fn;(void)name;(void)stack;(void)arg;(void)pri;(void)handle;return 1;
}
static inline int xTaskCreatePinnedToCoreWithCaps(void (*fn)(void *), const char *name, unsigned stack,
        void *arg, unsigned pri, void *handle, int core, unsigned caps) {
    (void)core; (void)caps; return xTaskCreate(fn,name,stack,arg,pri,handle);
}
static inline void vTaskDelay(unsigned ticks) { clock_us += ticks * 1000; }
static inline void *heap_caps_malloc(size_t n, int flags) { (void)flags;return malloc(n); }
static const char *http_response;
static size_t http_offset;
static int http_status=200;
static bool cancel_on_read;
void sw_tts_cancel(void);
static inline esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg) {
    if (strcmp(cfg->url,"https://openspeech.bytedance.com/api/v3/tts/unidirectional/sse") || !cfg->disable_auto_redirect) abort();
    http_offset=0;return (void *)1;
}
static inline int esp_http_client_set_header(void *h,const char *k,const char *v) {(void)h;(void)k;(void)v;return 0;}
static inline int esp_http_client_open(void *h,int len) {(void)h;(void)len;return 0;}
static inline int esp_http_client_write(void *h,const char *body,int len) {
    (void)h;if(!strstr(body,"\"sample_rate\":16000")||!strstr(body,"\"format\":\"pcm\""))abort();return len;
}
static inline int esp_http_client_fetch_headers(void *h) {(void)h;return 0;}
static inline int esp_http_client_get_status_code(void *h) {(void)h;return http_status;}
static inline int esp_http_client_read(void *h,char *p,int cap) {
    (void)h;(void)cap;if(cancel_on_read){cancel_on_read=false;sw_tts_cancel();}
    size_t n=strlen(http_response)-http_offset;if(n>7)n=7;memcpy(p,http_response+http_offset,n);http_offset+=n;return (int)n;
}
static inline int esp_http_client_get_errno(void *h) { (void)h; return 0; }
static inline int esp_http_client_close(void *h) {(void)h;return 0;}
static inline int esp_http_client_cleanup(void *h) {(void)h;return 0;}
/* Strict test decoder: real firmware links Mbed TLS. */
static inline int mbedtls_base64_decode(unsigned char *dst,size_t cap,size_t *olen,const unsigned char *src,size_t len) {
    const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if(len%4)return -1;size_t at=0;
    for(size_t i=0;i<len;i+=4){unsigned v=0,pad=0;
        for(unsigned j=0;j<4;j++){if(src[i+j]=='='){if(j<2)return -1;pad++;v<<=6;}else{const char *p=strchr(alphabet,src[i+j]);if(!p||pad)return -1;v=(v<<6)|(unsigned)(p-alphabet);}}
        if(pad>2||at+3-pad>cap)return -1;
        dst[at++]=(v>>16)&255;if(pad<2)dst[at++]=(v>>8)&255;if(!pad)dst[at++]=v&255;
    }*olen=at;return 0;
}

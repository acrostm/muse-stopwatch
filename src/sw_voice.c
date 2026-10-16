#include "sw_voice.h"
#include "sw_sign.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_CAP (96 * 1024)
#define AUDIO_CAP (512 * 1024)
typedef struct {
    bool preview;
    unsigned page;
    char ak[129], sk[257], date[17];
    sw_config_t config;
} voice_job_t;
static QueueHandle_t queue;
static SemaphoreHandle_t mutex;
static bool busy, audio;
static uint32_t identity;
static unsigned char *result;
static size_t result_size;
static int64_t expires;
static char error[160];

static esp_err_t json_reply(httpd_req_t *r, const char *status, const char *body)
{
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "Connection", "close");
    return httpd_resp_sendstr(r, body);
}
static bool read_field(cJSON *root, const char *name, char *out, size_t cap)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!v) return true;
    if (!cJSON_IsString(v) || !sw_header_safe(v->valuestring, cap - 1)) return false;
    if (*v->valuestring) sw_text_copy(out, cap, v->valuestring);
    return true;
}

esp_err_t sw_voice_submit(httpd_req_t *r, cJSON *root, bool preview)
{
    voice_job_t job = { .preview = preview, .page = 1 };
    if (preview) sw_config_get(&job.config);
    cJSON *page = cJSON_GetObjectItemCaseSensitive(root, "page");
    bool ok = cJSON_IsObject(root);
    if (preview) {
        ok = ok && read_field(root, "speaker", job.config.speaker, sizeof(job.config.speaker)) &&
            read_field(root, "api_key", job.config.api_key, sizeof(job.config.api_key)) &&
            read_field(root, "app_id", job.config.app_id, sizeof(job.config.app_id)) &&
            read_field(root, "access_key", job.config.access_key, sizeof(job.config.access_key)) &&
            job.config.speaker[0] && (job.config.api_key[0] || (job.config.app_id[0] && job.config.access_key[0]));
    } else {
        ok = ok && read_field(root, "ak", job.ak, sizeof(job.ak)) && read_field(root, "sk", job.sk, sizeof(job.sk)) &&
            read_field(root, "date", job.date, sizeof(job.date)) && job.ak[0] && job.sk[0] && job.date[0] &&
            cJSON_IsNumber(page) && page->valuedouble == page->valueint && page->valueint >= 1 && page->valueint <= 100;
        if (ok) job.page = page->valueint;
    }
    esp_err_t sent;
    if (!ok) sent = json_reply(r, "400 Bad Request", "{\"error\":\"Check voice and credentials. ListSpeakers requires cloud AK/SK; preview requires TTS credentials.\"}");
    else {
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (busy) sent = json_reply(r, "409 Conflict", "{\"error\":\"Another voice request is running\"}");
        else {
            free(result); result = NULL; result_size = 0; error[0] = 0;
            identity++; if (!identity) identity++;
            busy = true; audio = preview;
            if (xQueueSend(queue, &job, 0) != pdTRUE) {
                busy = false;
                sent = json_reply(r, "503 Service Unavailable", "{\"error\":\"Voice worker unavailable\"}");
            } else {
                char body[64]; snprintf(body, sizeof(body), "{\"job\":%lu}", (unsigned long)identity);
                sent = json_reply(r, "202 Accepted", body);
            }
        }
        xSemaphoreGive(mutex);
    }
    memset(&job, 0, sizeof(job));
    return sent;
}

esp_err_t sw_voice_result(httpd_req_t *r)
{
    char requested[16], expected[16];
    xSemaphoreTake(mutex, portMAX_DELAY);
    snprintf(expected, sizeof(expected), "%lu", (unsigned long)identity);
    esp_err_t sent;
    if (httpd_req_get_hdr_value_str(r, "X-Voice-Job", requested, sizeof(requested)) != ESP_OK || strcmp(requested, expected))
        sent = json_reply(r, "404 Not Found", "{\"error\":\"Voice request expired\"}");
    else if (busy) sent = json_reply(r, "202 Accepted", "{\"pending\":true}");
    else if (error[0]) {
        char body[200]; snprintf(body, sizeof(body), "{\"error\":\"%s\"}", error);
        sent = json_reply(r, "502 Bad Gateway", body);
    } else if (!result || esp_timer_get_time() >= expires)
        sent = json_reply(r, "410 Gone", "{\"error\":\"Voice result expired; retry\"}");
    else {
        httpd_resp_set_type(r, audio ? "application/octet-stream" : "application/json");
        httpd_resp_set_hdr(r, "Cache-Control", "no-store");
        httpd_resp_set_hdr(r, "Connection", "close");
        httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
        sent = httpd_resp_send(r, (const char *)result, result_size);
    }
    xSemaphoreGive(mutex);
    return sent;
}

static bool list(const voice_job_t *job, unsigned char *out, size_t *size, char *message, size_t cap)
{
    char body[128], hash[65], auth[512];
    snprintf(body, sizeof(body), "{\"ResourceIDs\":[\"seed-tts-2.0\"],\"Page\":%u,\"Limit\":50}", job->page);
    if (!sw_voice_sign(job->ak, job->sk, job->date, body, hash, auth, sizeof(auth))) {
        sw_text_copy(message, cap, "Invalid cloud AK/SK or timestamp"); return false;
    }
    esp_http_client_config_t cfg = {
        .url = "https://open.volcengineapi.com/?Action=ListSpeakers&Version=2025-05-20",
        .method = HTTP_METHOD_POST, .timeout_ms = 3000, .buffer_size = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach, .disable_auto_redirect = true,
    };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    bool ok = h != NULL;
    int status = 0;
    int64_t deadline = esp_timer_get_time() + 20LL * 1000000;
    if (h) {
        esp_http_client_set_header(h, "Content-Type", "application/json");
        esp_http_client_set_header(h, "X-Date", job->date);
        esp_http_client_set_header(h, "X-Content-Sha256", hash);
        esp_http_client_set_header(h, "Authorization", auth);
        ok = esp_http_client_open(h, strlen(body)) == ESP_OK;
        size_t at = 0;
        while (ok && at < strlen(body)) {
            int n = esp_http_client_write(h, body + at, strlen(body) - at);
            ok = n > 0 && esp_timer_get_time() < deadline;
            if (n > 0) at += n;
        }
        if (ok) { esp_http_client_fetch_headers(h); status = esp_http_client_get_status_code(h); ok = status == 200; }
        *size = 0;
        while (ok && !esp_http_client_is_complete_data_received(h)) {
            if (*size >= LIST_CAP - 1 || esp_timer_get_time() >= deadline) { ok = false; break; }
            int n = esp_http_client_read(h, (char *)out + *size, LIST_CAP - 1 - *size);
            if (n < 0 || (!n && !esp_http_client_is_complete_data_received(h))) { ok = false; break; }
            *size += n;
        }
        esp_http_client_close(h); esp_http_client_cleanup(h);
    }
    memset(auth, 0, sizeof(auth));
    if (!ok) {
        snprintf(message, cap, "ListSpeakers failed (HTTP %d). Check device Internet, cloud AK/SK, IAM permission and phone clock", status);
        return false;
    }
    out[*size] = 0;
    /* Parse/filter remote rows in the browser; a 50-row metadata tree can
     * exhaust scarce internal RAM even though the response itself is in PSRAM. */
    return true;
}

static void worker(void *arg)
{
    (void)arg;
    voice_job_t job;
    for (;;) {
        if (xQueueReceive(queue, &job, pdMS_TO_TICKS(5000)) != pdTRUE) {
            xSemaphoreTake(mutex, portMAX_DELAY);
            if (result && esp_timer_get_time() >= expires) { free(result); result = NULL; result_size = 0; }
            xSemaphoreGive(mutex); continue;
        }
        unsigned char *buffer = heap_caps_malloc(job.preview ? AUDIO_CAP : LIST_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        char message[160] = {0}; size_t size = 0;
        bool ok = buffer != NULL;
        if (!ok) strcpy(message, "Not enough memory; retry when device is idle");
        else if (job.preview) {
            /* Avoid starting an extra TLS request while dialogue is playing. */
            if (!sw_tts_done()) { ok = false; strcpy(message, "Dialogue is playing; retry when idle"); }
            else if (!sw_config_ready(&job.config)) { ok = false; strcpy(message, "No saved TTS credentials; enter API Key or App ID and Access Token"); }
            else ok = sw_tts_preview(&job.config, buffer, AUDIO_CAP, &size, message, sizeof(message));
            ESP_LOGI("sw_preview", "result=%s bytes=%u %s", ok ? "ok" : "failed", (unsigned)size, message);
        } else ok = list(&job, buffer, &size, message, sizeof(message));
        memset(&job, 0, sizeof(job));
        xSemaphoreTake(mutex, portMAX_DELAY);
        if (ok) { result = buffer; result_size = size; expires = esp_timer_get_time() + 60LL * 1000000; }
        else { free(buffer); sw_text_copy(error, sizeof(error), message); }
        busy = false;
        xSemaphoreGive(mutex);
    }
}

esp_err_t sw_voice_init(void)
{
    mutex = xSemaphoreCreateMutex(); queue = xQueueCreate(1, sizeof(voice_job_t));
    identity = esp_random();
    if (!mutex || !queue) return ESP_ERR_NO_MEM;
    return xTaskCreatePinnedToCoreWithCaps(worker, "voice_setup", 12 * 1024, NULL, 2, NULL,
        tskNO_AFFINITY, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool sw_voice_probe(void)
{
    voice_job_t job = { .preview = true };
    sw_config_get(&job.config);
    if (!sw_config_ready(&job.config)) { memset(&job, 0, sizeof(job)); return false; }
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool ok = !busy;
    if (ok) {
        free(result); result = NULL; result_size = 0; error[0] = 0;
        identity++; if (!identity) identity++;
        busy = true; audio = true;
        ok = xQueueSend(queue, &job, 0) == pdTRUE;
        if (!ok) busy = false;
    }
    xSemaphoreGive(mutex);
    memset(&job, 0, sizeof(job)); return ok;
}

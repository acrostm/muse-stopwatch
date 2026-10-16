#include "common.h"
#include "sw_runtime.h"
#include <assert.h>
void sw_config_get(sw_config_t *out) { memset(out,0,sizeof(*out));strcpy(out->api_key,"test-key");strcpy(out->speaker,"test-speaker"); }
void sw_tts_set_status(const char *s) { (void)s; }
#include "../src/sw_tts.c"

int main(void)
{
    assert(sw_tts_init()==ESP_OK);
    char *buffer=malloc(SW_LINE_CAP);unsigned char *decoded=malloc(SW_LINE_CAP*3/4);
    job_t job={.gen=atomic_load(&generation)};strcpy(job.text,"A response");
    http_response="event: 352\ndata: {\"code\":0,\"data\":\"AQACAAMA\"}\n\nevent: 152\ndata: {\"code\":20000000}\n\n";
    assert(synthesize(&job,buffer,decoded));
    unsigned char output[8];assert(sw_tts_read(output,0)==0);
    assert(sw_tts_read(output,sizeof(output))==6);
    assert(!memcmp(output,"\1\0\2\0\3\0",6));
    http_response="data: {\"code\":45000000,\"message\":\"denied\"}\n";
    assert(!synthesize(&job,buffer,decoded));
    http_response="data: {\"code\":0,\"data\":\"!!!!\"}\n";
    assert(!synthesize(&job,buffer,decoded));
    http_response="data: {\"code\":0,\"data\":\"AQ==\"}\n"; /* odd PCM length */
    assert(!synthesize(&job,buffer,decoded));
    http_response="data: {\"code\":0,\"data\":\"AQACAAMA\"}\n"; /* no final marker */
    assert(!synthesize(&job,buffer,decoded));sw_tts_cancel();job.gen=atomic_load(&generation);
    http_response="data: {\"code\":20000000}\n"; /* completion without audio */
    assert(!synthesize(&job,buffer,decoded));
    http_status=401;assert(!synthesize(&job,buffer,decoded));http_status=200;
    http_response="data: {\"code\":0,\"data\":\"AQACAAMA\"}\n\ndata: {\"code\":20000000}\n";
    cancel_on_read=true;assert(!synthesize(&job,buffer,decoded));assert(sw_tts_done());
    /* Preview has a private sink: it must not consume or reset conversation PCM. */
    http_response="data: {\"code\":0,\"data\":\"AQACAAMA\"}\n\ndata: {\"code\":20000000}\n";
    const unsigned char dialogue[2]={42,0};xStreamBufferSend(pcm,dialogue,2,0);
    unsigned char preview[8];size_t preview_size=0;char error[160]={0};sw_config_t config;sw_config_get(&config);
    assert(sw_tts_preview(&config,preview,sizeof(preview),&preview_size,error,sizeof(error)));
    assert(preview_size==6 && !memcmp(preview,"\1\0\2\0\3\0",6));
    assert(sw_tts_read(output,sizeof(output))==2 && output[0]==42);
    assert(!sw_tts_preview(&config,preview,2,&preview_size,error,sizeof(error)) && preview_size==0);
    http_response="data: {\"code\":0,\"data\":\"AQACAAMA\"}\n";
    assert(!sw_tts_preview(&config,preview,sizeof(preview),&preview_size,error,sizeof(error)));
    assert(strstr(error,"SSE stream incomplete"));
    http_status=401;
    assert(!sw_tts_preview(&config,preview,sizeof(preview),&preview_size,error,sizeof(error)));
    assert(strstr(error,"HTTP 401"));http_status=200;
    http_response="data: {\"code\":45000000,\"message\":\"secret detail must never be echoed\"}\n";
    assert(!sw_tts_preview(&config,preview,sizeof(preview),&preview_size,error,sizeof(error)));
    assert(strstr(error,"provider 45000000") && !strstr(error,"secret detail"));
    assert(sw_config_ready(&config));memset(&config,0,sizeof(config));assert(!sw_config_ready(&config));
    strcpy(config.speaker,"voice");strcpy(config.app_id,"app");assert(!sw_config_ready(&config));
    strcpy(config.access_key,"access");assert(sw_config_ready(&config));
    free(buffer);free(decoded);free(pcm);
    puts("tts: PASS");
}

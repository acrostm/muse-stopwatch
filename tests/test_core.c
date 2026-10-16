#include "sw_core.h"
#include "sw_net.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned lines;
static bool receive(const char *s, void *ctx)
{
    const char **expected = ctx;
    assert(!strcmp(s, expected[lines++]));
    return true;
}
static bool reject(const char *s, void *ctx) { (void)s; (void)ctx; return false; }

int main(void)
{
    struct sockaddr_in peer = { .sin_family = AF_INET }, local = { .sin_family = AF_INET };
    struct sockaddr_in6 peer6 = { .sin6_family = AF_INET6 }, local6 = { .sin6_family = AF_INET6 };
    inet_pton(AF_INET, "192.168.4.2", &peer.sin_addr);
    inet_pton(AF_INET, "192.168.4.1", &local.sin_addr);
    inet_pton(AF_INET6, "::ffff:192.168.4.2", &peer6.sin6_addr);
    inet_pton(AF_INET6, "::ffff:192.168.4.1", &local6.sin6_addr);
    uint32_t ap = local.sin_addr.s_addr, mask; inet_pton(AF_INET, "255.255.255.0", &mask);
#define ALLOWED(p,l) sw_setup_peer_allowed((const struct sockaddr *)&(p),sizeof(p), \
        (const struct sockaddr *)&(l),sizeof(l),ap,mask)
    assert(ALLOWED(peer,local));
    assert(ALLOWED(peer6,local6)); /* real HTTPD dual-stack accept representation */
    assert(ALLOWED(peer,local6) && ALLOWED(peer6,local));
    assert(!sw_setup_peer_allowed((const struct sockaddr *)&peer6, sizeof(peer),
            (const struct sockaddr *)&local6,sizeof(local6),ap,mask));
    inet_pton(AF_INET6, "fe80::1", &peer6.sin6_addr); assert(!ALLOWED(peer6,local6));
    inet_pton(AF_INET, "192.168.1.2", &peer.sin_addr); assert(!ALLOWED(peer,local));
    inet_pton(AF_INET, "192.168.4.2", &peer.sin_addr);
    inet_pton(AF_INET, "192.168.1.1", &local.sin_addr); assert(!ALLOWED(peer,local));
#undef ALLOWED
    char pin[9], saved_pin[9];
    assert(sw_setup_pin(pin, NULL, 1234567));
    assert(strlen(pin) == 8);
    memcpy(saved_pin, pin, sizeof(pin));
    assert(!sw_setup_pin(pin, saved_pin, 98765432));
    assert(!strcmp(pin, saved_pin)); /* fresh entropy on reboot never rotates a stored PIN */
    assert(sw_setup_pin(pin, "invalid", UINT32_MAX));
    for (unsigned i = 0; i < 8; ++i) assert(pin[i] >= '0' && pin[i] <= '9');
    assert(!sw_setup_pin(pin, pin, 0));
    char out[8];
    const char *s = "A\xe4\xb8\xad\xe6\x96\x87Z";
    for (size_t cap = 1; cap <= sizeof(out); ++cap) {
        memset(out, 0x55, sizeof(out));
        sw_text_copy(out, cap, s);
        assert(strlen(out) < cap);
        size_t n = strlen(out);
        assert(n == 0 || n == 1 || n == 4 || n == 7);
    }
    sw_text_copy(out, sizeof(out), "A\xe4\xb8"); assert(!strcmp(out, "A"));
    sw_history_t h = {0};
    for (unsigned i = 0; i < 20; ++i) {
        char text[16]; snprintf(text, sizeof(text), "message %u", i);
        sw_history_add(&h, i & 1, text);
    }
    assert(h.count == 8 && h.version == 20);
    assert(!strcmp(sw_history_get(&h, 0)->text, "message 19"));
    assert(!strcmp(sw_history_get(&h, 7)->text, "message 12"));
    assert(!sw_history_get(&h, 8));
    sw_history_add(&h, true, ""); assert(h.version == 20);
    assert(sw_header_safe("ordinary-key", 12));
    assert(!sw_header_safe("key\r\nInjected: yes", 100));
    assert(!sw_header_safe("long", 3));
    const char stream[] = "event: 352\r\ndata: {\"code\":0}\n\ndata: {\"code\":20000000}\n";
    const char *expected[] = { "event: 352", "data: {\"code\":0}", "", "data: {\"code\":20000000}" };
    for (size_t split = 0; split <= strlen(stream); ++split) {
        char buffer[80]; sw_lines_t p = { .buf = buffer, .cap = sizeof(buffer) }; lines = 0;
        assert(sw_lines_feed(&p, stream, split, receive, expected));
        assert(sw_lines_feed(&p, stream + split, strlen(stream) - split, receive, expected));
        assert(lines == 4 && !p.len);
    }
    char small[5]; sw_lines_t p = { .buf = small, .cap = sizeof(small) };
    assert(!sw_lines_feed(&p, "12345", 5, receive, expected));
    assert(!sw_lines_feed(&p, "\n", 1, receive, expected));
    p = (sw_lines_t){ .buf = small, .cap = sizeof(small) };
    assert(!sw_lines_feed(&p, "x\0y", 3, receive, expected));
    p = (sw_lines_t){ .buf = small, .cap = sizeof(small) };
    assert(!sw_lines_feed(&p, "x\n", 2, reject, NULL));
    sw_pairing_idle_t idle = {0};
    assert(!sw_pairing_idle_expired(&idle,0,true,false,false));
    assert(!sw_pairing_idle_expired(&idle,29999999,true,false,false));
    assert(sw_pairing_idle_expired(&idle,30000000,true,false,false));
    assert(!sw_pairing_idle_expired(&idle,30000001,true,false,false));
    assert(!sw_pairing_idle_expired(&idle,60000001,true,true,false));
    assert(!sw_pairing_idle_expired(&idle,90000001,true,true,false));
    assert(!sw_pairing_idle_expired(&idle,120000001,true,false,true));
    assert(!sw_pairing_idle_expired(&idle,130000001,false,false,false));
    assert(!sw_pairing_idle_expired(&idle,140000001,true,false,false));
    assert(!sw_pairing_idle_expired(&idle,139000001,true,false,false)); /* clock reset */
    puts("core: PASS");
}

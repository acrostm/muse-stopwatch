#include "sw_proxy_core.h"
#include <string.h>
#include <ctype.h>

bool sw_proxy_target(const char *host)
{
    if (!host) return false;
    size_t n = strlen(host);
    if (n && host[n-1] == '.') --n;
    const char *suffixes[] = { ".muse.ai", ".metaaivm.com" };
    for (unsigned i=0; i<2; ++i) {
        size_t len=strlen(suffixes[i]);
        if (n<=len) continue;
        bool match=true;
        for (size_t j=0;j<len;++j) if (tolower((unsigned char)host[n-len+j])!=suffixes[i][j]) match=false;
        if (match) return true;
    }
    return false;
}
bool sw_proxy_lan_ipv4(const char *host)
{
    if (!host) return false;
    unsigned octets[4]={0}; const char *p=host;
    for (unsigned i=0;i<4;++i) {
        unsigned digits=0;
        while (*p>='0'&&*p<='9') { if (++digits>3) return false; octets[i]=octets[i]*10+(*p++-'0'); }
        if (!digits||octets[i]>255) return false;
        if (i<3) { if (*p++!='.') return false; } else if (*p) return false;
    }
    return octets[0]==10 || (octets[0]==172&&octets[1]>=16&&octets[1]<=31) || (octets[0]==192&&octets[1]==168);
}
size_t sw_socks_request(const char *host, uint16_t port, unsigned char *out, size_t cap)
{
    if (!host || !out || !port) return 0;
    size_t n=strlen(host);
    if (!n||n>253||cap<n+7) return 0;
    for (size_t i=0;i<n;++i) if (!isalnum((unsigned char)host[i])&&host[i]!='.'&&host[i]!='-') return 0;
    out[0]=5;out[1]=1;out[2]=0;out[3]=3;out[4]=(unsigned char)n;
    memcpy(out+5,host,n);out[n+5]=port>>8;out[n+6]=port&255;
    return n+7;
}

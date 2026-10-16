import os
"""Check firmware canonical signing against an independent hashlib/HMAC oracle."""
import ctypes
import hashlib
import hmac
from pathlib import Path
import subprocess
import tempfile
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Signing(unittest.TestCase):
    def test_signature_and_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as tmp:
            temp = Path(tmp)
            (temp / 'mbedtls').mkdir()
            # macOS system crypto provides SHA256 and HMAC for the host shim.
            (temp / 'mbedtls/md.h').write_text('''
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
typedef int mbedtls_md_info_t;
#define MBEDTLS_MD_SHA256 1
static inline const int *mbedtls_md_info_from_type(int t){static int md; (void)t;return &md;}
static inline int mbedtls_md(const int *m,const unsigned char *s,size_t n,unsigned char *out){(void)m;CC_SHA256(s,(CC_LONG)n,out);return 0;}
static inline int mbedtls_md_hmac(const int *m,const unsigned char *k,size_t kn,const unsigned char *s,size_t n,unsigned char *out){(void)m;CCHmac(kCCHmacAlgSHA256,k,kn,s,n,out);return 0;}
''')
            libs = []
            if sys.platform != 'darwin':
                (temp / 'mbedtls/md.h').write_text('#include <openssl/sha.h>\n#include <openssl/hmac.h>\ntypedef int mbedtls_md_info_t;\n#define MBEDTLS_MD_SHA256 1\nstatic inline const int *mbedtls_md_info_from_type(int t){static int md;(void)t;return &md;}\nstatic inline int mbedtls_md(const int *m,const unsigned char *s,size_t n,unsigned char *out){(void)m;return SHA256(s,n,out)?0:-1;}\nstatic inline int mbedtls_md_hmac(const int *m,const unsigned char *k,size_t kn,const unsigned char *s,size_t n,unsigned char *out){(void)m;return HMAC(EVP_sha256(),k,(int)kn,s,n,out,NULL)?0:-1;}\n')
                libs = ['-lcrypto']
            lib = temp / 'sign.so' 
            subprocess.run([os.environ.get('CC', 'cc'),'-std=c11','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations',
                            '-shared','-fPIC','-I'+str(temp),'-I'+str(ROOT/'src'),
                            str(ROOT/'src/sw_sign.c'),str(ROOT/'src/sw_core.c'),'-o',str(lib)]+libs,check=True)
            fn=ctypes.CDLL(str(lib)).sw_voice_sign
            fn.argtypes=[ctypes.c_char_p]*6+[ctypes.c_size_t];fn.restype=ctypes.c_bool
            body=b'{"ResourceIDs":["seed-tts-2.0"],"Page":1,"Limit":50}'
            date=b'20261016T102030Z';day=b'20261016';scope=day+b'/cn-beijing/speech_saas_prod/request'
            payload=hashlib.sha256(body).hexdigest().encode()
            canonical=b'POST\n/\nAction=ListSpeakers&Version=2025-05-20\nhost:open.volcengineapi.com\nx-content-sha256:'+payload+b'\nx-date:'+date+b'\n\nhost;x-content-sha256;x-date\n'+payload
            to_sign=b'HMAC-SHA256\n'+date+b'\n'+scope+b'\n'+hashlib.sha256(canonical).hexdigest().encode()
            key=b'fixture-secret'
            for value in (day,b'cn-beijing',b'speech_saas_prod',b'request'):
                key=hmac.new(key,value,hashlib.sha256).digest()
            sig=hmac.new(key,to_sign,hashlib.sha256).hexdigest().encode()
            expected=b'HMAC-SHA256 Credential=fixture-ak/'+scope+b', SignedHeaders=host;x-content-sha256;x-date, Signature='+sig
            sha=ctypes.create_string_buffer(65);auth=ctypes.create_string_buffer(512)
            self.assertTrue(fn(b'fixture-ak',b'fixture-secret',date,body,sha,auth,512))
            self.assertEqual(sha.value,payload);self.assertEqual(auth.value,expected)
            for ak,sk,dt in [(b'ak\r\nX: bad',b'sk',date),(b'ak',b'',date),(b'ak',b'sk',b'20261016T102030'),(b'ak/extra',b'sk',date)]:
                self.assertFalse(fn(ak,sk,dt,body,sha,auth,512))
            self.assertFalse(fn(b'ak',b'sk',date,body,sha,auth,8))

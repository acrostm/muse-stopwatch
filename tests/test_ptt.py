"""Exercise physical hold boundaries and cancellation independently of audio."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PushToTalk(unittest.TestCase):
    def test_duration_cancel_and_reset(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'ptt.c'
            source.write_text('''#include "sw_core.h"
#include <assert.h>
int main(void) {
    sw_ptt_t s = {0};
    assert(sw_ptt_result(&s) == SW_PTT_WAIT);
    for (int duration = 0; duration <= 2000001; duration += 1) {
        sw_ptt_press(&s, 123456);
        assert(sw_ptt_result(&s) == SW_PTT_WAIT);
        sw_ptt_release(&s, 123456 + duration);
        assert(sw_ptt_result(&s) == (duration > 2000000 ? SW_PTT_SEND : SW_PTT_SHORT));
    }
    sw_ptt_press(&s, 1);
    sw_ptt_cancel(&s);
    sw_ptt_release(&s, 9000000);
    assert(sw_ptt_result(&s) == SW_PTT_CANCELLED);
    sw_ptt_release(&s, 9999999);
    assert(sw_ptt_result(&s) == SW_PTT_CANCELLED);
    sw_ptt_press(&s, 10000000);
    sw_ptt_release(&s, 14000000);
    assert(sw_ptt_result(&s) == SW_PTT_SEND);
    sw_ptt_press(&s, 14000000);
    sw_ptt_release(&s, 1);
    assert(sw_ptt_result(&s) == SW_PTT_SHORT);
    return 0;
}''')
            binary = Path(tmp) / 'ptt'
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-I'+str(ROOT/'src'), str(source),
                            str(ROOT/'src/sw_core.c'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

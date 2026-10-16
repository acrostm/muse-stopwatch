#!/usr/bin/env python3
"""Capture sample-only LVGL USB RGB565 frames as PNG without reset line toggles."""
import argparse
import base64
import os
from pathlib import Path
import re
import select
import struct
import termios
import time
import tty
import zlib

def capture(port, page):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        tty.setraw(fd)
        attr = termios.tcgetattr(fd); attr[2] &= ~termios.HUPCL; termios.tcsetattr(fd, termios.TCSANOW, attr)
        termios.tcflush(fd, termios.TCIFLUSH)
        os.write(fd, f'>sw.docs={page}\n'.encode())
        time.sleep(2)
        os.write(fd, b'p')
        pending = b''; chunks = []; width = height = 0
        deadline = time.monotonic()+60
        while time.monotonic() < deadline:
            if not select.select([fd], [], [], 0.2)[0]: continue
            pending += os.read(fd, 65536)
            while b'\n' in pending:
                line, pending = pending.split(b'\n', 1); line = line.strip()
                header = re.fullmatch(rb'SNAP BEGIN (\d+) (\d+) (\d+)', line)
                if header:
                    width, height, raw = map(int, header.groups())
                    if (width, height, raw) != (466, 466, 144): raise ValueError('Unexpected screenshot format')
                    chunks = []
                elif line == b'SNAP END' and width:
                    pcm = b''.join(chunks)
                    if len(pcm) != width*height*2: raise ValueError(f'Incomplete screenshot: {len(pcm)} bytes')
                    return width, height, pcm
                elif width and re.fullmatch(rb'[A-Za-z0-9+/]+={0,2}', line):
                    chunks.append(base64.b64decode(line, validate=True))
                    if sum(map(len, chunks)) > width*height*2: raise ValueError('Oversize screenshot')
        raise TimeoutError('No complete frame; build with CONFIG_LV_USE_SNAPSHOT=y')
    finally:
        os.close(fd)

def png(width, height, pcm):
    # Format conversion only: no cropping, masking or alteration of UI content.
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            value = struct.unpack_from('<H', pcm, (y*width+x)*2)[0]
            red, green, blue = value >> 11, (value >> 5) & 63, value & 31
            rows.extend((red*255//31, green*255//63, blue*255//31))
    def chunk(kind, data):
        return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data)&0xffffffff)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('port'); parser.add_argument('--page', type=int, choices=(1,2,3), required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(); args.out.write_bytes(png(*capture(args.port,args.page)))
    print('Saved sample UI capture:', args.out)

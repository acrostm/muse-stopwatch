#!/usr/bin/env python3
"""Fetch only the hash-verified cJSON 1.7.19 sources needed by host tests."""
import argparse
import hashlib
from pathlib import Path
import urllib.request

FILES = {
    'cJSON.c': '298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d',
    'cJSON.h': '25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee',
}
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=True)
    for name, expected in FILES.items():
        data = urllib.request.urlopen('https://raw.githubusercontent.com/DaveGamble/cJSON/v1.7.19/'+name, timeout=60).read()
        if hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit('cJSON source hash mismatch: '+name)
        (args.output/name).write_bytes(data)

#!/usr/bin/env python3
"""Standalone source gate; skipped tests and missing prerequisites fail closed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

def static():
    for path in ROOT.rglob('*.md'):
        if set(path.relative_to(ROOT).parts) & {'.git', 'build', 'out', 'firmware'}:
            continue
        text = path.read_text()
        partner = path.with_name(path.name.replace('.zh_CN.md', '.md')) if path.name.endswith('.zh_CN.md') else path.with_name(path.stem+'.zh_CN.md')
        if not partner.is_file():
            raise ValueError(f'Missing bilingual document: {partner.relative_to(ROOT)}')
        for target in re.findall(r'\]\(([^)]+)\)', text):
            if '://' in target or target.startswith(('#', 'mailto:')):
                continue
            local = target.split('#')[0]
            if local and not (path.parent / local).is_file():
                raise ValueError(f'Broken link: {path.relative_to(ROOT)} → {target}')
    for path in ROOT.rglob('*'):
        if not path.is_file() or set(path.relative_to(ROOT).parts) & {'.git', 'build', 'out', 'firmware', '__pycache__'}:
            continue
        if path.suffix in {'.png'}:
            continue
        data = path.read_text()
        if re.search(r'^<{7} |^>{7} ', data, re.M):
            raise ValueError(f'Merge conflict: {path}')
        if re.search(r'mgst_[A-Za-z0-9]{32,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----', data):
            raise ValueError(f'Possible private credential: {path}')
    patch = (ROOT/'upstream.patch').read_text()
    touched = set(re.findall(r'^--- a/(.+)$', patch, re.M))
    if touched != set(json.loads((ROOT/'upstream-hashes.json').read_text())):
        raise ValueError('Upstream hash coverage differs from patch')
    html = (ROOT/'src/sw_portal.html').read_text()
    script = '\n'.join(re.findall(r'<script>(.*?)</script>', html, re.S))
    with tempfile.TemporaryDirectory() as tmp:
        file = Path(tmp)/'portal.js'; file.write_text(script)
        subprocess.run(['node', '--check', str(file)], check=True)
    print('Static checks: PASS', flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', type=Path)
    args = parser.parse_args()
    static()
    suite = unittest.defaultTestLoader.discover(str(ROOT/'tests'), pattern='test_*.py')
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    if not result.wasSuccessful() or result.skipped or not result.testsRun:
        raise SystemExit('Host gate failed (skips are not accepted)')
    if args.firmware:
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([sys.executable, str(ROOT/'package.py'), 'create', str(args.firmware), '--archive-root', tmp], check=True)
        print('Firmware verification: PASS')

if __name__ == '__main__':
    main()

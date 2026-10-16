#!/usr/bin/env python3
"""Build and run adaptation pages against the pinned SDK's actual headless LVGL."""
import argparse
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('lvgl_source', type=Path)
    args = p.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['cmake', '-S', str(ROOT/'tests/ui'), '-B', tmp,
                        '-DLVGL_SOURCE_DIR='+str(args.lvgl_source.resolve())], check=True)
        subprocess.run(['cmake', '--build', tmp, '--parallel', '4'], check=True)
        subprocess.run([str(Path(tmp)/'test_pages')], check=True, timeout=30)

#!/bin/bash
# Build the distributable addon zip.
set -e
cd "$(dirname "$0")"
OUT="entro_cycles_$(date +%Y%m%d).zip"
rm -f "$OUT"
zip -r "$OUT" entro_cycles/ -x '*.pyc' '__pycache__/*'
echo "Created $OUT"

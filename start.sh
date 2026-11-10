#!/bin/sh
set -eu
cd "$(dirname "$0")"
cd fm1
. ./env.sh
mkdir -p build/gen
"$PYTHON" tools/gen_tables.py build/gen/felucca_tables.h
"$PYTHON" tools/gen_font.py build/gen/felucca_font.h
"$PYTHON" tools/import_ht_audio.py
cc -O2 -std=c99 -ffp-contract=off -Wno-unused-function -DOM_HOST -Ibuild/gen -Ifirmware/src -Ifirmware/src/dsp tools/face-host.c -o build/face-host
cd ..
"$PYTHON" tools/prepare_capture.py
exec "$PYTHON" serve.py

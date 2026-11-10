#!/bin/sh
set -eu
cd "$(dirname "$0")/fm1"
. ./env.sh
mkdir -p build/gen
"$PYTHON" tools/gen_tables.py build/gen/felucca_tables.h
"$PYTHON" tools/gen_font.py build/gen/felucca_font.h
"$PYTHON" tools/import_ht_audio.py
cc -O1 -g -std=c99 -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer -DOM_HOST -Ibuild/gen -Ifirmware/src -Ifirmware/src/dsp tools/face-host.c -o build/face-host-sanitize
./build/face-host-sanitize --check
cc -O1 -g -std=c99 -ffp-contract=off -fsanitize=address,undefined -fno-omit-frame-pointer tools/feed-test.c -o build/feed-test-sanitize
./build/feed-test-sanitize
cc -O2 -g -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/ht_adpcm_test.c -o build/ht-adpcm-test
mkdir -p build/audio-decoded
./build/ht-adpcm-test build/audio-decoded
"${AUDIO_TEST_PYTHON:-$PYTHON}" tests/test_ht_audio.py --decoded-dir build/audio-decoded
cc -O2 -g -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen tests/ht_audio_test.c -o build/ht-audio-test
cc -O2 -g -std=c11 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Ifirmware/src -Ibuild/gen tests/ht_ambient_test.c -o build/ht-ambient-test
./build/ht-ambient-test
exec ./build/ht-audio-test

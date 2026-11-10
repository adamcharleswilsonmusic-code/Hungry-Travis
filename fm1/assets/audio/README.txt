Hungry Travis character recordings

Originals:13 user-supplied Adam Wilson recordings copied unchanged from
assets/audio/originals. Mono48kHz24-bitPCM;34.389250s total.
Original file hashes and formats are in ht_voice_manifest.json. The publication copy uses a project-relative source path. These WAVs are archival inputs and are not linked into firmware.

Firmware assets:
ht_voice_bank.ima:385,792B, mono22,050Hz canonical4-bitIMA WAV blocks.
Each256-byteblock starts LE16predictor,U8index,U8reserved0 and contains504
low-nibble-first deltas, for505samples. Final blocks are padded; sample_count
preserves exact playback end.13 clips yield758,282validsamples=34.389206s.
ht_voice_envelopes.bin:1,725B of50Hz RMS, normalized0..255perclip.

Semantic order is fixed: happy1..3 IDs0..2, angry1..5 IDs3..7, zen1..5 IDs8..12.
No clip is trimmed. Processing removes DC, uses64-tap Blackman sinc anti-alias
resampling, sets peak-3dBFS with gain capped4x, and applies5ms endpoint fades.
The manifest records all processing, encoded/decoded hashes, offsets and SNR.

Normal build, no external dependencies:
python3 tools/import_ht_audio.py
This validates durable assets and emits build/gen/ht_voice_bank.h, with const
bank data, descriptors and envelopes for direct XIP use. No full-bank RAM copy.

Recreate encoded assets from preserved originals (maintainer action):
python3 tools/import_ht_audio.py --source assets/audio/originals
To record new Desktop provenance, use the original source directory instead.
Existing archival files with different bytes are never overwritten silently.
Source path in manifest can change with import location; binary output is stable.

Runtime header firmware/src/dsp/ht_adpcm.h:
ht_voice_clip_t {const uint8_t *data;uint32_t encoded_bytes,sample_count,sample_rate;}
ht_adpcm_start(state,clip):1valid,0invalid; checks shape/count before reading.
ht_adpcm_next(state):next PCM16 source sample,0inactive; errors silence and stop.
ht_adpcm_stop(state):stop. State.active,error,position are available to mixer.
State is32B on64-bithost, expected28B on32-bitFM1; no heap or blockbuffer.
Mixer handles44.1kHz interpolation/output, fades, priorities and event mapping.
ht_voice_envelope_at(clip_id,source_sample_position) gives0..255mouth target.

Host verification (from project root):
mkdir -p output/audio-capacity/decoded
clang -std=c11 -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror -Ifm1/firmware/src -Ifm1/build/gen fm1/tests/ht_adpcm_test.c -o output/audio-capacity/ht-adpcm-test
output/audio-capacity/ht-adpcm-test output/audio-capacity/decoded
/usr/bin/python3 fm1/tests/test_ht_audio.py --decoded-dir output/audio-capacity/decoded --reimport

Python3.9 system audioop independently verifies every decoded sample. Tests cover
malformed descriptors/headers, exact ends, short/finalblocks,10,000randomstreams,
anti-alias rejection, originals unchanged, header reproducibility and full source
reimport reproducibility. Real-device decoder/renderer/USB validation is separate.

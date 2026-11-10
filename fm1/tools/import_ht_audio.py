#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Preserve WAV originals; build fixed IMA assets and reproducible const header.

Import: python3 tools/import_ht_audio.py --source '/path/Hungry Travis'
Build:  python3 tools/import_ht_audio.py
No third-party dependencies. Existing checked-in encoded assets are validated
and expanded into build/gen/ht_voice_bank.h by the normal build invocation.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'assets/audio'
RATE, BLOCK, SAMPLES, CAP = 22050, 256, 505, 384 * 1024
NAMES = [f'{kind} {i}' for kind, count in [('happy', 3), ('angry', 5), ('zen', 5)] for i in range(1, count + 1)]
STEPS = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
INDEX = [-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8]
def sha(data): return hashlib.sha256(data).hexdigest()

def decode_delta(code, predictor, index):
    step = STEPS[index]
    diff = (step >> 3) + ((step >> 2) if code & 1 else 0) + ((step >> 1) if code & 2 else 0) + (step if code & 4 else 0)
    predictor = max(-32768, min(32767, predictor + (-diff if code & 8 else diff)))
    return predictor, max(0, min(88, index + INDEX[code]))

def encode_pcm(pcm):
    """Canonical IMA WAV, explicit predictor and carried index per independent block."""
    encoded, decoded, index = bytearray(), [], 0
    for start in range(0, len(pcm), SAMPLES):
        real = pcm[start:start + SAMPLES]
        block = real + [real[-1]] * (SAMPLES - len(real))
        predictor = block[0]
        encoded += struct.pack('<hBB', predictor, index, 0)
        dec = [predictor]
        codes = []
        for sample in block[1:]:
            delta = sample - predictor
            code = 8 if delta < 0 else 0
            diff, step = abs(delta), STEPS[index]
            if diff >= step: code |= 4; diff -= step
            if diff >= step >> 1: code |= 2; diff -= step >> 1
            if diff >= step >> 2: code |= 1
            predictor, index = decode_delta(code, predictor, index)
            codes.append(code); dec.append(predictor)
        encoded.extend(codes[i] | codes[i+1] << 4 for i in range(0, 504, 2))
        decoded.extend(dec[:len(real)])
    return bytes(encoded), decoded

def read_pcm(path):
    with wave.open(str(path), 'rb') as w:
        channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        if channels != 1 or width not in (2, 3) or w.getcomptype() != 'NONE' or not frames or rate < RATE:
            raise ValueError(f'{path}: require nonempty mono PCM16/24 WAV, rate >=22050')
        raw = w.readframes(frames)
    if len(raw) != frames*channels*width:
        raise ValueError(f'{path}: truncated PCM data')
    scale = float(1 << (width*8-1))
    pcm = [int.from_bytes(raw[i:i+width], 'little', signed=True)/scale for i in range(0, len(raw), width)]
    return pcm, dict(channels=channels, bits=width*8, sample_rate=rate, sample_count=frames, seconds=frames/rate)

def resample(pcm, input_rate):
    """64-tap Blackman-windowed sinc, phase-normalized; preserve full duration."""
    if input_rate == RATE: return list(pcm)
    factor = math.gcd(input_rate, RATE)
    denominator, advance = RATE // factor, input_rate // factor
    cutoff = 0.5 * RATE / input_rate * 0.94
    phases = []
    for phase in range(denominator):
        fraction = phase / denominator
        weights = []
        for k in range(-31, 33):
            x = k - fraction
            window = 0.42 + 0.5*math.cos(math.pi*x/32) + 0.08*math.cos(2*math.pi*x/32) if abs(x) <= 32 else 0.0
            weights.append((math.sin(2*math.pi*cutoff*x)/(math.pi*x) if x else 2*cutoff) * window)
        norm = sum(weights)
        phases.append([w/norm for w in weights])
    result = []
    for i in range(round(len(pcm)*RATE/input_rate)):
        integer, phase = divmod(i*advance, denominator)
        weights = phases[phase]
        result.append(sum(pcm[j]*w for j,w in zip(range(integer-31, integer+33), weights) if 0 <= j < len(pcm)))
    return result

def prepare(pcm):
    dc = sum(pcm)/len(pcm)
    centered = [v-dc for v in pcm]
    peak = max(abs(v) for v in centered)
    if peak < 0.0001: raise ValueError('effectively silent recording')
    gain = min(4.0, 10**(-3/20)/peak)
    fade = max(1, round(RATE*0.005))
    out = [max(-32768,min(32767,round(v*gain*min(1.0,i/fade,(len(pcm)-1-i)/fade)*32767))) for i,v in enumerate(centered)]
    return out, dict(dc_removed=dc, linear_gain=gain, peak_pcm16=max(abs(v) for v in out))

def envelope(pcm):
    rms = []
    for i in range(math.ceil(len(pcm)*50/RATE)):
        chunk = pcm[i*RATE//50:min(len(pcm),(i+1)*RATE//50)]
        rms.append(math.sqrt(sum(v*v for v in chunk)/len(chunk)) if chunk else 0)
    peak = max(rms, default=1) or 1
    return bytes(min(255,round(255*r/peak)) for r in rms)

def import_source(folder):
    bank, envelopes, records = bytearray(), bytearray(), []
    pending_originals = []
    for ident, name in enumerate(NAMES):
        source = folder / (name + '.wav')
        original = source.read_bytes()
        target = ASSETS / 'originals' / source.name
        if target.exists() and target.read_bytes() != original:
            raise ValueError(f'Preserved original differs: {target}; refusing overwrite')
        raw, fmt = read_pcm(source)
        pcm, stats = prepare(resample(raw, fmt['sample_rate']))
        encoded, decoded = encode_pcm(pcm)
        env = envelope(decoded)
        mse = sum((a-b)**2 for a,b in zip(pcm,decoded))/len(pcm)
        power = sum(a*a for a in pcm)/len(pcm)
        records.append(dict(id=ident, name=name, semantic_id='HT_VOICE_'+name.replace(' ','').upper(),
            original_file='originals/'+source.name, original_sha256=sha(original), original_bytes=len(original),
            original_format=fmt, offset=len(bank), encoded_bytes=len(encoded), sample_count=len(pcm), sample_rate=RATE,
            encoded_sha256=sha(encoded), decoded_pcm16_sha256=sha(struct.pack('<'+'h'*len(decoded),*decoded)),
            envelope_offset=len(envelopes), envelope_count=len(env), processing=stats,
            decode_snr_db=10*math.log10(power/mse) if mse else None))
        bank.extend(encoded); envelopes.extend(env); pending_originals.append((source,target))
    if len(bank) > CAP: raise ValueError(f'Voice payload {len(bank)} >{CAP}; nothing written')
    manifest = dict(format='hungry-travis-ima-wav-v1', license='User-supplied Adam Wilson character recordings; original source files retained unchanged',
        source_folder=str(folder.resolve()), sample_rate=RATE, channels=1, block_bytes=BLOCK, block_samples=SAMPLES,
        envelope_hz=50, bank_bytes=len(bank), bank_cap_bytes=CAP, bank_sha256=sha(bank),
        envelope_bytes=len(envelopes), envelope_sha256=sha(envelopes),
        original_seconds=sum(c['original_format']['seconds'] for c in records),
        decoded_seconds=sum(c['sample_count']/RATE for c in records),
        processing='Full duration; DC removal; 64-tap Blackman sinc anti-alias resampling, cutoff94% target Nyquist; peak-3dBFS with gain<=4;5ms fades; canonical IMA4; decoded50Hz RMS normalized per clip', clips=records)
    (ASSETS/'originals').mkdir(parents=True, exist_ok=True)
    for source,target in pending_originals:
        if not target.exists(): shutil.copy2(source,target)
    (ASSETS/'ht_voice_bank.ima').write_bytes(bank)
    (ASSETS/'ht_voice_envelopes.bin').write_bytes(envelopes)
    (ASSETS/'ht_voice_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest

def generate_header(destination):
    manifest = json.loads((ASSETS/'ht_voice_manifest.json').read_text())
    bank = (ASSETS/'ht_voice_bank.ima').read_bytes()
    env = (ASSETS/'ht_voice_envelopes.bin').read_bytes()
    if len(bank)>CAP or len(bank)!=manifest['bank_bytes'] or len(env)!=manifest['envelope_bytes'] or sha(bank)!=manifest['bank_sha256'] or sha(env)!=manifest['envelope_sha256']:
        raise ValueError('Voice bank/envelope length or integrity check failed')
    clips=manifest['clips']
    if len(clips)!=13: raise ValueError('Need13 clips in fixed semantic order')
    lines=['/* SPDX-License-Identifier: GPL-3.0-only */','/* Generated by tools/import_ht_audio.py; originals/provenance in assets/audio. */','#pragma once','#include "dsp/ht_adpcm.h"',
           '#define HT_VOICE_COUNT 13u','#define HT_VOICE_ENVELOPE_HZ 50u','enum { '+', '.join(f"{c['semantic_id']}={i}" for i,c in enumerate(clips))+' };']
    for label,data in [('ht_voice_data',bank),('ht_voice_envelope_data',env)]:
        lines.append(f'static const uint8_t {label}[{len(data)}]={{')
        lines.extend(','.join(str(x) for x in data[i:i+32])+',' for i in range(0,len(data),32))
        lines.append('};')
    lines.append('static const ht_voice_clip_t ht_voice_clips[HT_VOICE_COUNT]={')
    expected_offset=0
    for i,c in enumerate(clips):
        o,n=c['offset'],c['encoded_bytes']
        if c['id']!=i or c['name']!=NAMES[i] or c['sample_rate']!=RATE or c['sample_count']<1 or n!=(c['sample_count']+504)//505*256 or o!=expected_offset or o+n>len(bank) or sha(bank[o:o+n])!=c['encoded_sha256']:
            raise ValueError('Invalid voice descriptor')
        expected_offset=o+n
        lines.append(f"{{ht_voice_data+{o}u,{n}u,{c['sample_count']}u,{RATE}u}},")
    if expected_offset!=len(bank): raise ValueError('Unreferenced voice bank bytes')
    lines.extend(['};','typedef struct { const uint8_t *data; uint32_t count; } ht_voice_envelope_t;','static const ht_voice_envelope_t ht_voice_envelopes[HT_VOICE_COUNT]={'])
    expected_offset=0
    for c in clips:
        o,n=c['envelope_offset'],c['envelope_count']
        if o!=expected_offset or n!=(c['sample_count']*50+RATE-1)//RATE or o+n>len(env): raise ValueError('Invalid envelope descriptor')
        expected_offset=o+n
        lines.append(f'{{ht_voice_envelope_data+{o}u,{n}u}},')
    if expected_offset!=len(env): raise ValueError('Unreferenced envelope bytes')
    lines.extend(['};','static inline uint8_t ht_voice_envelope_at(uint32_t id,uint32_t source_sample_position){',
        ' if(id>=HT_VOICE_COUNT || source_sample_position>=ht_voice_clips[id].sample_count)return 0;',
        ' uint32_t i=(source_sample_position/441u); /*22050Hz/50Hz exact*/',
        ' return i<ht_voice_envelopes[id].count?ht_voice_envelopes[id].data[i]:0;','}'])
    text='\n'.join(lines)+'\n'
    destination.parent.mkdir(parents=True,exist_ok=True)
    if not destination.exists() or destination.read_text()!=text: destination.write_text(text)
    return manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,help='Import/copy original WAVs and rebuild durable encoded assets')
    parser.add_argument('--out-header',type=Path,default=ROOT/'build/gen/ht_voice_bank.h')
    args=parser.parse_args()
    if args.source: import_source(args.source)
    m=generate_header(args.out_header)
    print(f"Voices:13 clips,{m['original_seconds']:.6f}s originals,{m['decoded_seconds']:.6f}s playback,{m['bank_bytes']}/{CAP} encoded B; envelope{m['envelope_bytes']}B")
if __name__=='__main__': main()

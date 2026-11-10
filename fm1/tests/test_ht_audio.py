#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Host importer/asset/reference parity tests. C test emits its actual PCM first.
Run with the system Python3.9 for the independent stdlib audioop reference:
 /usr/bin/python3 fm1/tests/test_ht_audio.py --decoded-dir output/audio-capacity/decoded
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import tempfile

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ht_import',ROOT/'tools/import_ht_audio.py')
imp=importlib.util.module_from_spec(spec);spec.loader.exec_module(imp)

def reference_decode(data,count):
    import audioop
    out=bytearray()
    for start in range(0,len(data),256):
        block=data[start:start+256]
        predictor,index,reserved=struct.unpack_from('<hBB',block)
        assert len(block)==256 and index<=88 and reserved==0
        # audioop uses high nibble first; IMA WAV packs low first.
        swap=bytes((v>>4)|((v&15)<<4) for v in block[4:])
        pcm,state=audioop.adpcm2lin(swap,2,(predictor,index))
        out+=struct.pack('<h',predictor)+pcm
    return bytes(out[:count*2])

def main():
    p=argparse.ArgumentParser();p.add_argument('--decoded-dir',type=Path,required=True)
    p.add_argument('--reimport',action='store_true',help='Also reimport all preserved originals into a temporary directory')
    args=p.parse_args()
    assets=ROOT/'assets/audio';m=json.loads((assets/'ht_voice_manifest.json').read_text());bank=(assets/'ht_voice_bank.ima').read_bytes()
    assert m['bank_bytes']==len(bank)<=393216 and hashlib.sha256(bank).hexdigest()==m['bank_sha256']
    assert len(m['clips'])==13 and abs(m['original_seconds']-34.38925)<1e-8
    aggregate=0
    for c in m['clips']:
        original=(assets/c['original_file']).read_bytes();assert hashlib.sha256(original).hexdigest()==c['original_sha256']
        source=Path(m['source_folder'])/Path(c['original_file']).name
        if source.exists(): assert source.read_bytes()==original
        data=bank[c['offset']:c['offset']+c['encoded_bytes']]
        reference=reference_decode(data,c['sample_count'])
        actual=(args.decoded_dir/f"voice-{c['id']}.pcm").read_bytes()
        assert actual==reference,(c['name'],'C/reference mismatch')
        assert hashlib.sha256(reference).hexdigest()==c['decoded_pcm16_sha256']
        aggregate+=c['sample_count']
    with tempfile.TemporaryDirectory() as temp:
        header=Path(temp)/'bank.h';imp.generate_header(header)
        assert header.read_bytes()==(ROOT/'build/gen/ht_voice_bank.h').read_bytes()
        if args.reimport:
            prior=imp.ASSETS
            try:
                imp.ASSETS=Path(temp)/'audio'
                again=imp.import_source(assets/'originals')
                assert (imp.ASSETS/'ht_voice_bank.ima').read_bytes()==bank
                assert (imp.ASSETS/'ht_voice_envelopes.bin').read_bytes()==(assets/'ht_voice_envelopes.bin').read_bytes()
                # Python3.12+ uses compensated float sums; diagnostic floating
                # metrics may differ below machine precision across host versions.
                # Exact encoded audio, envelopes, decoded hashes and descriptors
                # must remain identical.
                for old,new in zip(m['clips'],again['clips']):
                    assert {k:v for k,v in old.items() if k not in ('processing','decode_snr_db')}=={k:v for k,v in new.items() if k not in ('processing','decode_snr_db')}
                    assert abs(old['decode_snr_db']-new['decode_snr_db'])<1e-9
                    for k in old['processing']:
                        assert abs(old['processing'][k]-new['processing'][k])<1e-9
            finally: imp.ASSETS=prior
    # Resampler pass-band and anti-alias behavior; test on tones away from fades.
    def tone(freq):return [0.5*math.sin(2*math.pi*freq*i/48000) for i in range(4800)]
    def rms(x):return math.sqrt(sum(v*v for v in x)/len(x))
    low=rms(imp.resample(tone(2400),48000)[100:-100])
    rejected=rms(imp.resample(tone(15000),48000)[100:-100])
    assert abs(low-0.5/math.sqrt(2))<0.002
    assert rejected/low<0.01,('anti-alias rejection',rejected/low)
    for n in (1,2,504,505,506,1010,1011):
        pcm=[round(25000*math.sin(i*0.1)) for i in range(n)]
        encoded,decoded=imp.encode_pcm(pcm)
        assert len(encoded)==((n+504)//505)*256
        assert reference_decode(encoded,n)==struct.pack('<'+'h'*n,*decoded)
    print(f'PASS originals/hash/full durations,all13 C/audioop PCM parity,{aggregate} samples,reproducible header,anti-alias filter,short blocks')
    snr=','.join('{:.1f}'.format(c['decode_snr_db']) for c in m['clips'])
    print(f'Bank{len(bank)}B; duration{aggregate/22050:.6f}s; SNR dB per clip: '+snr)
if __name__=='__main__':main()

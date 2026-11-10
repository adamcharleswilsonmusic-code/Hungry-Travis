# SPDX-License-Identifier: GPL-3.0-only
import struct,subprocess,unittest
import mido
from capture import decode,digest,NativeRenderer,BIN,REVISION,STATE_BYTES

def packet(raw):
 p=[125,72,84,1,REVISION&127,REVISION>>7,1,0]+[(digest(raw)>>(7*i))&127 for i in range(5)]
 for i in range(0,len(raw),7):
  group=raw[i:i+7];p.append(sum(((v>>7)&1)<<k for k,v in enumerate(group)));p.extend(v&127 for v in group)
 return mido.Message('sysex',data=p)
class CaptureTests(unittest.TestCase):
 def test_authoritative_states(self):
  renderer=NativeRenderer()
  try:
   for n in (0,1,3,9,30):
    raw=subprocess.check_output([str(BIN),'--snapshot-fixture',str(n),'.7']);self.assertEqual(len(raw),STATE_BYTES);self.assertEqual(len(packet(raw).data),201);self.assertAlmostEqual(struct.unpack_from('<f',raw,160)[0],.7);self.assertEqual(decode(packet(raw)),raw);self.assertEqual(len(renderer.render(raw)),115200)
  finally:renderer.close()
 def test_speech_pixels_and_invalid_amplitude(self):
  renderer=NativeRenderer()
  try:
   calm=subprocess.check_output([str(BIN),'--snapshot-fixture','0','0'])
   speech=subprocess.check_output([str(BIN),'--snapshot-fixture','0','.9'])
   self.assertNotEqual(renderer.render(calm),renderer.render(speech))
   for amplitude in (float('nan'),float('inf'),float('-inf'),-.1,1.1):
    invalid=bytearray(calm);struct.pack_into('<f',invalid,160,amplitude)
    with self.assertRaisesRegex(ValueError,'speech mouth'):decode(packet(invalid))
  finally:renderer.close()
 def test_damage_and_wrong_revision(self):
  raw=subprocess.check_output([str(BIN),'--snapshot-fixture','1']);p=list(packet(raw).data)
  for mutation in ('short','version','checksum'):
   v=p.copy()
   if mutation=='short':v.pop()
   elif mutation=='version':v[4]^=1
   else:v[-1]^=1
   with self.assertRaises(ValueError):decode(mido.Message('sysex',data=v))
  self.assertIsNone(decode(mido.Message('note_on',note=60)))
if __name__=='__main__':unittest.main()

import sys,json,time,subprocess,datetime,hashlib
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'tools'))
import fm1_install
out=Path('build/device-test/797');backend=fm1_install.MidoBackend();up=fm1_install.Updater(backend,'Felucca');start=time.monotonic();samples=[]
for target in (0,10,20,30):
 time.sleep(max(0,start+target-time.monotonic()));dev=up.find();s={'elapsed_s':target,'identity':dev.id.text if dev else None,'port':dev.name if dev else None};samples.append(s);print(json.dumps(s),flush=True)
 if dev:dev.link.close()
(out/'runtime-identity.json').write_text(json.dumps(samples,indent=2)+'\n')
r=subprocess.run(['system_profiler','SPAudioDataType','-json'],capture_output=True,text=True,timeout=20);audio=json.loads(r.stdout) if r.returncode==0 else {'error':r.stderr};(out/'audio-enumeration.json').write_text(json.dumps(audio,indent=2)+'\n')
def find(v):
 if isinstance(v,dict):
  if 'FM-1' in str(v.get('_name','')):return v
  for x in v.values():
   a=find(x)
   if a:return a
 elif isinstance(v,list):
  for x in v:
   a=find(x)
   if a:return a
 return None
a=find(audio);midi_ok=all(s['identity']=='FM-1_797' for s in samples);audio_ok=bool(a and a.get('coreaudio_device_input')==2 and a.get('coreaudio_device_srate')==44100)
p=json.loads((out/'package-validation.json').read_text());hash_ok=hashlib.sha256(Path(p['path']).read_bytes()).hexdigest()==p['sha256']
report={'identity':'FM-1_797','installed':midi_ok,'verified_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'package_hash_matches_validation':hash_ok,'all_identity_checks_passed':midi_ok,'observations':samples,'usb_audio_enumeration_passed':audio_ok,'audio_device':a,'scope':'Installation/reboot, four MIDI identity checks over30seconds, USB audio enumeration. Hungry Travis native face renderer tested on host; device controls and measured rendering speed are separate checks. Previous game restore package preserved.'}
(out/'verification-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True);sys.exit(0 if midi_ok and audio_ok and hash_ok else 1)

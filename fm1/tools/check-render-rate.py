"""Read-only renderer/audio counters from the native FM-1; no game input."""
import argparse,json,time
from pathlib import Path
import mido
p=argparse.ArgumentParser();p.add_argument('--seconds',type=int,default=15);p.add_argument('--out',default='output/device-render-rate.json');a=p.parse_args()
fields=['uptime_ms','frames','render_total_us','render_max_us','render_last_us','audio_cpu_pct','audio_xruns','clipped_samples','mode']
observations=[]
with mido.open_input('Felucca') as inp,mido.open_output('Felucca') as out:
 for _ in range(a.seconds+1):
  out.send(mido.Message('control_change',channel=15,control=118,value=1));end=time.monotonic()+1;sample=None
  while time.monotonic()<end:
   m=inp.poll()
   if m is not None and m.type=='sysex' and tuple(m.data[:4])==(0x7d,0x44,0x43,1) and len(m.data)==49:
    sample={field:sum(m.data[4+i*5+k]<<(7*k) for k in range(5))&0xffffffff for i,field in enumerate(fields)}
   time.sleep(.003)
  if sample:
   if observations:
    old=observations[-1];dt=(sample['uptime_ms']-old['uptime_ms'])&0xffffffff;df=(sample['frames']-old['frames'])&0xffffffff;dr=(sample['render_total_us']-old['render_total_us'])&0xffffffff
    if dt and df:sample.update(fps=round(df*1000/dt,2),render_avg_ms=round(dr/df/1000,3))
   observations.append(sample);print(json.dumps(sample),flush=True)
Path(a.out).parent.mkdir(parents=True,exist_ok=True);Path(a.out).write_text(json.dumps(observations,indent=2)+'\n')
if len(observations)<2:raise SystemExit('No usable performance window')

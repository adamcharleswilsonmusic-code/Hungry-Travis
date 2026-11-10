# SPDX-License-Identifier: GPL-3.0-only
"""Authoritative FM-1 follower and local movie recorder. Never sends game input."""
import array,hashlib,json,math,os,secrets,shutil,struct,subprocess,threading,time
from datetime import datetime
from pathlib import Path
import mido
ROOT=Path(__file__).resolve().parents[1]
REVISION=797
STATE_BYTES=164
PIXEL_BYTES=115200
BIN=ROOT/'fm1/build/face-host'
HELPERS=ROOT/'fm1/build/recorder'

def digest(raw):
 h=2166136261
 for b in raw:h=((h^b)*16777619)&0xffffffff
 return h

def decode(message):
 if message.type!='sysex':return None
 p=bytes(message.data)
 if p[:4]!=b'\x7dHT\x01':return None
 if len(p)!=13+STATE_BYTES+(STATE_BYTES+6)//7:raise ValueError('Incomplete device state')
 if p[4]|p[5]<<7!=REVISION:raise ValueError('This device needs the matching Hungry Travis update')
 packed=p[13:];raw=bytearray();i=0
 while i<len(packed):
  bits=packed[i];i+=1
  for k in range(min(7,len(packed)-i)):
   raw.append(packed[i]|(((bits>>k)&1)<<7));i+=1
 if len(raw)!=STATE_BYTES or digest(raw)!=sum(p[8+k]<<(k*7) for k in range(5)):raise ValueError('Damaged device state')
 if struct.unpack_from('<II',raw)!=(0x48545331,REVISION):raise ValueError('State version mismatch')
 talk=struct.unpack_from('<f',raw,160)[0]
 if not math.isfinite(talk) or not 0<=talk<=1:raise ValueError('Invalid speech mouth amplitude')
 return bytes(raw)

def read_exact(pipe,n):
 chunks=[]
 while n:
  b=pipe.read(n)
  if not b:raise RuntimeError('Native renderer stopped; rebuild the matching test')
  chunks.append(b);n-=len(b)
 return b''.join(chunks)

class NativeRenderer:
 def __init__(self,path=BIN):
  self.process=subprocess.Popen([str(path),'--snapshot-stream'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,bufsize=0)
 def render(self,state):
  self.process.stdin.write(state);self.process.stdin.flush();return read_exact(self.process.stdout,PIXEL_BYTES)
 def close(self):
  self.process.terminate();self.process.wait(timeout=3);self.process.stdin.close();self.process.stdout.close()

class LiveBridge:
 def __init__(self):
  self.lock=threading.RLock();self.requested=False;self.thread=None;self.latest=None;self.rgb=None;self.received=0;self.frames=0;self.corrupt=0;self.message='Connect FM-1 to follow the actual console.';self.started=0;self.recorder=None
 def connect(self):
  with self.lock:
   if self.thread and self.thread.is_alive():return
   self.requested=True;self.frames=0;self.started=time.monotonic();self.message='Connecting to FM-1…';self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
 def disconnect(self):
  self.requested=False
  if self.thread:self.thread.join(timeout=4)
 def status(self):
  with self.lock:
   fresh=self.rgb is not None and time.monotonic()-self.received<2
   return {'requested':self.requested,'connected':fresh,'frames':self.frames,'corrupt':self.corrupt,'message':self.message,'seconds':round(time.monotonic()-self.started,1) if self.started else 0,'state':struct.unpack_from('<I',self.latest,144)[0] if self.latest else None}
 def frame(self):
  with self.lock:
   if self.rgb is None or time.monotonic()-self.received>2:raise RuntimeError(self.message)
   return self.rgb,self.frames
 def run(self):
  renderer=NativeRenderer()
  try:
   while self.requested:
    inp=out=None
    try:
     inputs=[x for x in mido.get_input_names() if 'Felucca' in x or 'FM-1' in x];outputs=[x for x in mido.get_output_names() if 'Felucca' in x or 'FM-1' in x]
     if len(inputs)!=1 or len(outputs)!=1:raise RuntimeError('Connect one FM-1 by USB. Retrying…')
     inp=mido.open_input(inputs[0]);out=mido.open_output(outputs[0]);lease=0;last=time.monotonic()
     while self.requested:
      now=time.monotonic()
      if now-lease>=1:out.send(mido.Message('control_change',channel=15,control=119,value=1));lease=now
      message=inp.poll()
      if message is None:
       if now-last>3:raise RuntimeError('No complete matching device frames. Reconnecting…')
       time.sleep(.001);continue
      try:state=decode(message)
      except ValueError as e:
       self.corrupt+=1;self.message=str(e);continue
      if state is None:continue
      pixels=renderer.render(state);last=now
      with self.lock:self.latest=state;self.rgb=pixels;self.received=now;self.frames+=1;self.message='Following the actual FM-1. Play on the console.'
      if self.recorder:self.recorder.feed(now,state)
    except Exception as exc:
     with self.lock:self.received=0;self.message=str(exc)
     if self.recorder:self.recorder.connection_lost()
     for _ in range(10):
      if not self.requested:break
      time.sleep(.1)
    finally:
     if out:
      try:out.send(mido.Message('control_change',channel=15,control=119,value=0))
      except Exception:pass
      out.close()
     if inp:inp.close()
  finally:
   renderer.close()
   with self.lock:self.received=0;self.message='Disconnected. Connect FM-1 when ready.'

class Recorder:
 def __init__(self,bridge):
  self.bridge=bridge;bridge.recorder=self;self.lock=threading.RLock();self.worker=None;self.state={'status':'ready','message':'Connect FM-1, then press Record.','frames':0,'seconds':0,'output':'','destination':str(Path.home()/'Desktop')};self.file=None;self.stop_event=threading.Event()
 def status(self):
  with self.lock:return dict(self.state)
 def update(self,**kwargs):
  with self.lock:self.state.update(kwargs)
 def start(self,validation=False):
  with self.lock:
   if self.worker and self.worker.is_alive():raise ValueError('A recording or export is already running.')
   if not self.bridge.status()['connected']:raise ValueError('Connect the live FM-1 first.')
   if not (HELPERS/'audio').exists() or not (HELPERS/'export').exists():raise ValueError('Recording helpers need preparation. Run start.sh again.')
   stamp=datetime.now().strftime('%Y-%m-%d_%H-%M-%S_%f');self.folder=ROOT/'recordings'/('take-'+stamp);self.folder.mkdir(parents=True)
   self.destination=self.folder/'validation.mov' if validation else Path.home()/'Desktop'/('Hungry-Travis-'+stamp+'.mov')
   shutil.copy2(BIN,self.folder/'renderer');self.file=(self.folder/'frames.bin').open('wb');self.started=time.monotonic();self.first=None;self.last=None;self.count=0;self.error='';self.stop_event.clear()
   self.state={'status':'starting','message':'Opening FM-1 USB audio…','frames':0,'seconds':0,'output':str(self.destination),'destination':str(self.destination.parent)}
   self.worker=threading.Thread(target=self.run,daemon=True);self.worker.start()
 def feed(self,now,raw):
  with self.lock:
   if self.state['status'] not in ('starting','recording') or self.stop_event.is_set():return
   self.file.write(struct.pack('<d',now)+raw);self.count+=1;self.first=self.first or now;self.last=now;self.state.update(frames=self.count,seconds=round(now-self.started,1))
 def stop(self):
  with self.lock:
   if self.state['status'] not in ('starting','recording'):raise ValueError('No active recording.')
   self.state.update(status='stopping',message='Stopping and exporting the movie…');self.stop_event.set()
 def connection_lost(self):
  with self.lock:
   if self.state['status'] in ('starting','recording'):
    self.error='FM-1 disconnected during recording. The received portion is preserved.';self.stop_event.set()
 def run(self):
  audio=None
  try:
   with (self.folder/'audio.log').open('w') as log:
    audio=subprocess.Popen([str(HELPERS/'audio'),str(self.folder/'usb-audio.mov'),'86400',str(self.folder/'STOP')],stdout=log,stderr=subprocess.STDOUT)
    while not self.stop_event.wait(.1):
     if audio.poll() is not None:raise RuntimeError('FM-1 USB audio could not be recorded. No microphone fallback was used; see the take’s audio log.')
     if (self.folder/'usb-audio.mov.ready').exists():self.update(status='recording',message='Recording the actual FM-1 picture and its USB audio.')
     elif time.monotonic()-self.started>10:raise RuntimeError('FM-1 audio access is not ready. Check macOS audio permission, then retry.')
   self.finish_audio(audio);audio=None
   with self.lock:self.file.close();self.file=None
   if not self.count:raise RuntimeError('No device frames were recorded.')
   audio_meta=json.loads((self.folder/'usb-audio.mov.json').read_text())
   if audio_meta.get('writer_status')!=2 or audio_meta.get('samples',0)<=0:raise RuntimeError('The USB audio track was not saved. Raw device states are preserved.')
   capture={'revision':REVISION,'state_bytes':STATE_BYTES,'frames':self.count,'end_host_seconds':time.monotonic(),'error':self.error,'renderer_sha256':hashlib.sha256((self.folder/'renderer').read_bytes()).hexdigest(),'scope':'Authoritative device render inputs, matching frozen native C renderer, no replayed controls or interpolated motion.'}
   (self.folder/'capture.json').write_text(json.dumps(capture,indent=2)+'\n');self.update(status='exporting',message='Exporting movie with USB audio…');self.export()
   self.update(status='complete',message=(self.error+' ' if self.error else '')+'Movie exported. Ready for another take.')
  except Exception as exc:self.update(status='error',message=str(exc))
  finally:
   if audio:
    try:self.finish_audio(audio)
    except Exception:pass
   with self.lock:
    if self.file:self.file.close();self.file=None
 def finish_audio(self,audio):
  (self.folder/'STOP').touch()
  try:audio.wait(timeout=15)
  except subprocess.TimeoutExpired:audio.terminate();audio.wait(timeout=5)
  if audio.returncode:raise RuntimeError('USB audio recording failed. Raw states and the audio log are preserved in the take folder.')
 def export(self):
  # Stream one frame at a time through the exact take-frozen C renderer.
  lut=[bytes([(i&31)*255//31,((i>>5)&63)*255//63,((i>>11)&31)*255//31,255]) for i in range(65536)]
  renderer=NativeRenderer(self.folder/'renderer')
  try:
   with (self.folder/'export.log').open('w') as log:
    process=subprocess.Popen([str(HELPERS/'export'),str(self.folder)],stdin=subprocess.PIPE,stdout=log,stderr=subprocess.STDOUT)
    try:
     with (self.folder/'frames.bin').open('rb') as src:
      while True:
       stamp=src.read(8)
       if not stamp:break
       raw=src.read(STATE_BYTES)
       if len(stamp)!=8 or len(raw)!=STATE_BYTES:raise RuntimeError('Incomplete saved device state')
       rgb=renderer.render(raw);words=array.array('H',rgb)
       if os.sys.byteorder=='little':words.byteswap()
       process.stdin.write(stamp);process.stdin.write(b''.join(lut[x] for x in words))
     process.stdin.close();rc=process.wait(timeout=120)
     if rc:raise RuntimeError('Movie export failed. Raw frames and audio remain in the take folder.')
    except Exception:
     process.terminate();process.wait(timeout=5);raise
  finally:renderer.close()
  meta=json.loads((self.folder/'playthrough.mov.json').read_text())
  if meta['actual_device_frames']!=self.count:raise RuntimeError('Export frame count differs from saved states.')
  temp=self.destination.with_suffix('.mov.partial')
  with temp.open('xb') as dst,(self.folder/'playthrough.mov').open('rb') as src:shutil.copyfileobj(src,dst)
  os.link(temp,self.destination);temp.unlink()

class CaptureService:
 def __init__(self):
  self.token=secrets.token_urlsafe(32);self.bridge=LiveBridge();self.recorder=Recorder(self.bridge)
 def status(self):return {'live':self.bridge.status(),'recording':self.recorder.status(),'token':self.token}
 def close(self):
  if self.recorder.status()['status'] in ('starting','recording'):self.recorder.stop()
  if self.recorder.worker:self.recorder.worker.join(timeout=30)
  self.bridge.disconnect()

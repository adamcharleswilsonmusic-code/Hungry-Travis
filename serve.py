#!/usr/bin/env python3
"""Local FM-1 native-renderer test surface; no GPU or chip-emulator claim."""
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from urllib.parse import urlsplit,parse_qs
from pathlib import Path
import subprocess,threading,math,os,json,shutil
from tools.capture import CaptureService
ROOT=Path(__file__).resolve().parent
class Renderer:
 def __init__(self):
  self.lock=threading.Lock()
  self._spawn()
 def _spawn(self):
  executable=ROOT/'fm1/build/face-host'
  self.version=executable.stat().st_mtime_ns
  self.process=subprocess.Popen([str(executable),'--game-stream'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,bufsize=0)
 def close(self):
  self.process.terminate();self.process.wait(timeout=3)
  self.process.stdin.close();self.process.stdout.close()
 def frame(self,args):
  with self.lock:
   if (ROOT/'fm1/build/face-host').stat().st_mtime_ns!=self.version:
    self.close();self._spawn()
   self.process.stdin.write((' '.join(str(x) for x in args)+'\n').encode());self.process.stdin.flush()
   chunks=[];left=115200
   while left:
    data=self.process.stdout.read(left)
    if not data:raise RuntimeError('Native renderer exited')
    chunks.append(data);left-=len(data)
   return b''.join(chunks)
renderer=None
capture=None
class Handler(SimpleHTTPRequestHandler):
 def __init__(self,*a,**kw):super().__init__(*a,directory=str(ROOT/'web'),**kw)
 def log_message(self,fmt,*a):
  if not self.path.startswith(('/frame.rgb','/capture/')):super().log_message(fmt,*a)
 def do_GET(self):
  parsed=urlsplit(self.path)
  if parsed.path=='/capture/status':
   body=json.dumps(capture.status()).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
  elif parsed.path=='/capture/frame.rgb':
   try:
    body,sequence=capture.bridge.frame();self.send_response(200);self.send_header('Content-Type','application/octet-stream');self.send_header('Cache-Control','no-store');self.send_header('X-Frame-Sequence',str(sequence));self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
   except RuntimeError as exc:self.send_error(503,str(exc))
  elif parsed.path=='/capture/movie' and capture.recorder.status()['status']=='complete':
   path=Path(capture.recorder.status()['output']);self.send_response(200);self.send_header('Content-Type','video/quicktime');self.send_header('Content-Length',str(path.stat().st_size));self.end_headers()
   with path.open('rb') as f:shutil.copyfileobj(f,self.wfile)
  elif parsed.path=='/frame.rgb':
   try:
    q=parse_qs(parsed.query);values=[float(q.get(k,[d])[0]) for k,d in [('yaw',0),('pitch',0),('mouth',.22),('time',0)]]
    if not all(math.isfinite(v) and abs(v)<100000 for v in values):raise ValueError('Invalid parameter')
    values += [int(q.get('keys',[0])[0])&63,int(q.get('reset',[0])[0])&1,int(q.get('paused',[0])[0])&1,int(q.get('face',[0])[0])&1,float(q.get('dt',[.04])[0])]
    if not math.isfinite(values[-1]):raise ValueError('Invalid frame interval')
    body=renderer.frame(values);self.send_response(200);self.send_header('Content-Type','application/octet-stream');self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
   except (ValueError,RuntimeError,BrokenPipeError) as e:self.send_error(503,str(e))
  elif parsed.path=='/reference/drawing.png':
   body=(ROOT/'reference/drawing.png').read_bytes();self.send_response(200);self.send_header('Content-Type','image/png');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
  else:super().do_GET()
 def do_POST(self):
  if self.headers.get('Host') not in ('127.0.0.1:8780','localhost:8780') or self.headers.get('X-Capture-Token')!=capture.token or self.headers.get('Origin','http://127.0.0.1:8780') not in ('http://127.0.0.1:8780','http://localhost:8780'):
   self.send_error(403,'Use the local Hungry Travis controls.');return
  try:
   if self.path=='/capture/connect':capture.bridge.connect()
   elif self.path=='/capture/disconnect':
    if capture.recorder.status()['status'] in ('starting','recording','stopping','exporting'):raise ValueError('Stop the recording and wait for export first.')
    capture.bridge.disconnect()
   elif self.path=='/capture/record':capture.recorder.start()
   elif self.path=='/capture/stop':capture.recorder.stop()
   else:raise ValueError('Unknown action')
   data=capture.status();code=200
  except (ValueError,RuntimeError) as exc:data={'error':str(exc)};code=409
  body=json.dumps(data).encode();self.send_response(code);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
if __name__=='__main__':
 renderer=Renderer();capture=CaptureService();server=ThreadingHTTPServer(('127.0.0.1',8780),Handler)
 print('Hungry Travis native C prototype: http://127.0.0.1:8780/',flush=True)
 try:server.serve_forever()
 finally:capture.close();renderer.close();server.server_close()

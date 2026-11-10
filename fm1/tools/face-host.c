/* SPDX-License-Identifier: GPL-3.0-only
 * Host harness for the exact native C face renderer and framebuffer. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../firmware/src/app/plat.h"
static uint32_t host_ms,host_keys,host_buttons,host_led_buttons;
static const uint16_t*pending;
static uint16_t submitted[240*240];
static void lcd_sync(void){if(pending)assert(memcmp(pending,submitted,sizeof submitted)==0);pending=0;}
static void lcd_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h,const uint16_t*p){(void)x;(void)y;assert(w==240&&h==240);lcd_sync();pending=p;memcpy(submitted,p,sizeof submitted);}
static void lcd_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint16_t c){(void)x;(void)y;(void)w;(void)h;(void)c;}
static int32_t clamp(int32_t v,int32_t l,int32_t h){return v<l?l:v>h?h:v;}
#include "../firmware/src/gfx.c"
uint32_t plat_ms(void){return host_ms;}uint32_t plat_keys(void){return host_keys;}uint32_t plat_buttons(void){return host_buttons;}int32_t plat_enc(int role){(void)role;return 0;}
void plat_glow(int a){(void)a;}void plat_play_red(int a){(void)a;}void plat_leds(uint32_t b,uint32_t k){host_led_buttons=b;(void)k;}
#include "../firmware/src/dsp/omni.c"
#include "../firmware/src/app/ui.c"
#include "../firmware/src/app/ht_snapshot.h"
/* Local interactive simulation advances its actual native audio callback.
 * Snapshot/frozen-take rendering never calls this or advances any sound. */
static void host_audio_advance(unsigned frames){
 static int32_t samples[256*2];
 while(frames){unsigned n=frames>256?256:frames;om_render(samples,n,4096);frames-=n;}
}
static void frame(float yaw,float pitch,float mouth,float time){ht_yaw=fm_clampf(yaw,-1.5f,1.5f);ht_pitch=fm_clampf(pitch,-.4f,.4f);ht_mouth_value=fm_clampf(mouth,0,1);ht_time=time;ht_draw();}
/* Control/physics fixtures explicitly begin by pressing the real GLO key.
 * ui_init itself remains the player-facing empty READY state. */
static void host_start_loaded(void){
 host_buttons=0;ui_init();host_buttons=1u<<B_GLO;ui_input_only();host_buttons=0;ui_input_only();assert(ht_feed.state==HT_LOADED);
}
int main(int argc,char**argv){
 om_init();ui_init();
 if(argc>1&&!strcmp(argv[1],"--check")){
  /* Boot waits indefinitely for readiness: no bean motion, failed bites or
   * anger, even after a minute, held movement and the former SAVE button. */
  host_buttons=host_keys=0;ui_init();
  assert(ht_feed.state==HT_SPILLED&&ht_feed.served==0&&ht_feed.spilled==0&&ht_feed.anger==0&&ht_feed.frustration==0);
  float ready_x=ht_feed.food_x,ready_y=ht_feed.food_y,ready_z=ht_feed.food_z;
  for(int i=0;i<600;i++){
   host_ms+=100;ui_input_only();assert(ht_feed.state==HT_SPILLED&&ht_feed.reach==0&&ht_feed.roll==0&&ht_feed.pitch==.22f);
   assert(ht_feed.food_x==ready_x&&ht_feed.food_y==ready_y&&ht_feed.food_z==ready_z&&ht_feed.served==0&&ht_feed.spilled==0&&ht_feed.anger==0&&ht_feed.frustration==0);
  }
  assert(host_led_buttons&(1u<<B_GLO));host_keys=(1u<<26)|(1u<<0)|(1u<<1);host_buttons=1u<<B_SAVE;
  for(int i=0;i<20;i++){host_ms+=10;ui_input_only();assert(ht_feed.state==HT_SPILLED&&ht_feed.reach==0&&ht_feed.roll==0&&ht_feed.pitch==.22f&&ht_feed.served==0&&ht_feed.spilled==0&&ht_feed.frustration==0);}
  host_buttons=host_keys=0;host_ms++;ui_input_only();host_buttons=1u<<B_GLO;host_ms++;ui_input_only();
  assert(ht_feed.state==HT_LOADED&&ht_feed.food_x==.025f&&ht_feed.food_y==.13f&&ht_feed.reach==0&&ht_feed.spilled==0&&ht_feed.frustration==0);
  host_buttons=0;host_keys=1u<<26;host_ms+=100;ui_input_only();assert(ht_feed.reach>0&&ht_feed.state==HT_LOADED);
  host_buttons=host_keys=0;ui_init();
  for(int i=0;i<180;i++){ht_feed.anger=(i%11)*.1f;frame(-1.5f+(float)(i%61)*.05f,-.4f+(float)(i%17)*.05f,(float)(i%11)*.1f,(float)i*.05f);}
  /* Reward extremes: no out-of-bounds raster writes or leaked spoon coverage. */
  for(int i=0;i<80;i++){ht_feed.state=HT_FED;ht_feed.served=1+i%9;ht_feed.return_time=(i/80.0f)*ht_reward_duration();frame((i%3-1)*.7f,0,.1f,0);assert(ht_coverage==16&&ht_eye_roll>=0&&ht_eye_roll<=1);}
  host_start_loaded();host_ms+=100;ui_input_only();
  host_buttons=1u<<B_PLAY;host_ms+=8;ui_input_only();ht_feed_t frozen=ht_feed;
  host_buttons=0;host_keys=1u<<26;for(int i=0;i<120;i++){host_ms+=8;ui_input_only();}
  assert(!memcmp(&frozen,&ht_feed,sizeof frozen));
  host_buttons=1u<<B_PLAY;host_ms+=8;ui_input_only();host_buttons=0;host_ms+=100;ui_input_only();assert(ht_feed.reach>0);
  host_buttons=1u<<B_GLO;host_ms+=1;ui_input_only();assert(ht_feed.reach<.001f&&ht_feed.state==HT_LOADED);
  /* GLO is the physical reload key. SAVE cannot refill a missed bite, even
   * held while advancing; GLO refills exactly once and preserves frustration. */
  host_buttons=host_keys=0;ui_init();ht_feed.anger=ht_feed.frustration=.5f;ht_feed.served=2;ht_feed_miss();
  host_keys=1u<<26;host_buttons=1u<<B_SAVE;
  for(int i=0;i<30;i++){host_ms+=10;ui_input_only();assert(ht_feed.state==HT_SPILLED&&ht_feed.reach==0&&ht_feed.spilled==1&&ht_feed.frustration==.75f);}
  assert(host_led_buttons&(1u<<B_GLO));
  host_buttons=host_keys=0;host_ms++;ui_input_only();float reload_anger=ht_feed.anger;
  host_buttons=1u<<B_GLO;host_ms++;ui_input_only();
  assert(ht_feed.state==HT_LOADED&&ht_feed.food_x==.025f&&ht_feed.food_y==.13f&&ht_feed.reach==0);
  assert(ht_feed.served==2&&ht_feed.spilled==1&&ht_feed.frustration==.75f&&ht_feed.anger==reload_anger&&!(host_led_buttons&(1u<<B_GLO)));
  assert(text_w(&FONT_HINT,"PRESS GLO FOR NEW BEAN")<=238);
  for(int i=0;i<30;i++){float x=i*.01f-.15f,y=.14f,z=.08f;ht_spoon_xyz a=ht_spoon_transform(x,y,z,0,-1.10f,1.45f,ht_feed.roll,ht_feed.pitch);ht_vec3 b=ht_feed_world(x,y,z);assert(fm_fabsf(a.x-b.x)<.00001f&&fm_fabsf(a.y-b.y)<.00001f&&fm_fabsf(a.z-b.z)<.00001f);}
  host_buttons=0;host_keys=1u<<0;host_start_loaded();host_ms+=100;ui_input_only();assert(ht_feed.roll>0);
  host_keys=1u<<2;host_start_loaded();host_ms+=100;ui_input_only();assert(ht_feed.roll<0);
  host_keys=1u<<1;host_start_loaded();host_ms+=100;ui_input_only();assert(ht_feed.pitch<.22f);
  host_keys=1u<<25;host_start_loaded();host_ms+=100;ui_input_only();assert(ht_feed.pitch>.22f);
  host_keys=1u<<24;host_start_loaded();ht_feed.reach=.5f;host_ms+=100;ui_input_only();assert(ht_feed.reach<.5f);
  host_keys=1u<<26;host_start_loaded();ht_feed.reach=.5f;host_ms+=100;ui_input_only();assert(ht_feed.reach>.5f);
  ht_face_set_anger(0);int calm_g=ht_skin.g;ht_face_set_anger(1);assert(ht_skin.g<calm_g&&ht_skin.r>=232&&ht_eye.g==250);ht_face_set_anger(0);
  ht_spoon_set_food(0);ht_material first=ht_spoon_food;ht_spoon_set_food(1);assert(ht_spoon_food.r!=first.r||ht_spoon_food.g!=first.g);ht_spoon_set_food(0);
  /* Angry deformation must retain two visible whites/pupils and the physical
   * mouth opening, rather than burying features in the changing skin mesh. */
  for(int miss=0;miss<=4;miss++){
   ui_init();ht_feed.anger=ht_feed.frustration=miss*.25f;frame(0,0,.18f,0);
   for(int side=0;side<2;side++){
    int whites=0,darks=0;
    for(int y=110;y<155;y++)for(int x=side?120:65;x<(side?178:118);x++){
     unsigned c=swap16(cv_px[y*240+x]);int r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
     if(r>120&&g>120&&b>100&&abs(r-g)<18)whites++;
     if(r<45&&g<45&&b<45)darks++;
    }assert(whites>20&&darks>5);
   }
  }
  /* No success title at any tier: with the effect just starting, the upper
   * caption rows are the untouched background. Paused reward still says PAUSED. */
  for(int feed=1;feed<=10;feed++){
   ui_init();ht_feed.served=feed;ht_feed.state=HT_FED;ht_feed.return_time=0;frame(0,0,.1f,0);
   for(int i=0;i<240*20;i++)assert(cv_px[i]==swap16(RGB(23,36,34)));
  }
  /* Pause must also freeze hunger and the automatic withdrawal. */
  host_keys=0;host_buttons=0;ui_init();ht_feed.wait_time=12;ht_feed.anger=.6f;ht_feed.frustration=0;ht_feed.state=HT_FED;ht_feed.return_reach=ht_feed.reach=.8f;
  host_buttons=1u<<B_PLAY;host_ms+=1;ui_input_only();frozen=ht_feed;host_buttons=0;for(int i=0;i<100;i++){host_ms+=10;ui_input_only();}assert(!memcmp(&frozen,&ht_feed,sizeof frozen));
  /* Every transition phase stays frozen under pause, including the full image. */
  for(int phase=0;phase<4;phase++){
   host_buttons=host_keys=0;ui_init();ht_feed.state=HT_FED;ht_feed.served=5;
   const float phases[]={1.8f,HT_CYCLE_ZEN_SECONDS+.2f,HT_CYCLE_ZEN_SECONDS+HT_CYCLE_MORPH_SECONDS+.4f,HT_MAX_REWARD_SECONDS-.01f};
   ht_feed.return_time=phases[phase];
   host_buttons=1u<<B_PLAY;host_ms++;ui_input_only();ht_draw();static uint16_t paused_pixels[240*240];memcpy(paused_pixels,cv_px,sizeof paused_pixels);frozen=ht_feed;
   host_buttons=0;host_keys=~0u;host_ms+=100;ui_input_only();ht_draw();assert(!memcmp(&frozen,&ht_feed,sizeof frozen)&&!memcmp(paused_pixels,cv_px,sizeof paused_pixels));
   assert(ht_model_scale==1&&ht_model_x==0&&ht_model_y==0&&ht_model_z==0&&!ht_portrait_warp&&!ht_features);
  }
  /* The authoritative voice envelope moves native mouth pixels without
   * modifying feeding physics. Approach, Zen and face-study retain ownership. */
  static uint16_t speech_pixels[240*240];
  host_buttons=host_keys=0;ui_init();ht_time=0;ht_talk_value=0;ht_draw();memcpy(speech_pixels,cv_px,sizeof speech_pixels);frozen=ht_feed;
  ht_talk_value=.8f;ht_draw();int speech_changes=0;
  for(int y=160;y<205;y++)for(int x=85;x<155;x++)if(speech_pixels[y*240+x]!=cv_px[y*240+x])speech_changes++;
  assert(speech_changes>100&&!memcmp(&frozen,&ht_feed,sizeof frozen));
  ht_feed.reach=1;ht_talk_value=0;ht_draw();memcpy(speech_pixels,cv_px,sizeof speech_pixels);ht_talk_value=1;ht_draw();assert(!memcmp(speech_pixels,cv_px,sizeof speech_pixels));
  ht_feed.state=HT_FED;ht_feed.served=5;ht_feed.return_time=2;ht_talk_value=0;ht_draw();memcpy(speech_pixels,cv_px,sizeof speech_pixels);ht_talk_value=1;ht_draw();assert(!memcmp(speech_pixels,cv_px,sizeof speech_pixels));
  ht_test_mode=0;ht_mouth_value=.35f;ht_talk_value=0;ht_draw();memcpy(speech_pixels,cv_px,sizeof speech_pixels);ht_talk_value=1;ht_draw();assert(!memcmp(speech_pixels,cv_px,sizeof speech_pixels));
  host_buttons=host_keys=0;ui_init();ht_talk_value=.7f;host_buttons=1u<<B_PLAY;host_ms++;ui_input_only();ht_draw();memcpy(speech_pixels,cv_px,sizeof speech_pixels);
  host_buttons=0;for(int i=0;i<10;i++){host_ms+=50;ui_input_only();ht_draw();assert(ht_talk_value==.7f&&!memcmp(speech_pixels,cv_px,sizeof speech_pixels));}
  /* Advance the actual native startup voice only in this explicit test.
   * The displayed amplitude must follow its nonzero decoder envelope. */
  host_buttons=host_keys=0;om_init();ui_init();float voice_peak=0;
  for(int i=0;i<240;i++){
   host_audio_advance(256);host_ms+=6;ui_input_only();
   assert(ht_talk_value==fm_clampf(ht_audio_mouth(),0,1));
   voice_peak=fm_maxf(voice_peak,ht_talk_value);
  }assert(voice_peak>.02f);
  /* Authoritative snapshots reproduce the exact C image without stepping physics. */
  static uint16_t expected[240*240];uint8_t snapshot[HT_SNAPSHOT_BYTES];
  for(int i=0;i<32;i++){
   ui_init();ht_yaw=(i%5-2)*.45f;ht_pitch=(i%3-1)*.2f;ht_time=i*.37f;ht_motion=i%2;ht_feed.state=i%4;ht_feed.served=i%10;ht_feed.return_time=((i%9)/9.0f)*HT_MAX_REWARD_SECONDS;ht_feed.reach=(i%5)*.2f;ht_feed.anger=(i%4)*.25f;ht_talk_value=(i%7)/6.0f;
   ht_draw();memcpy(expected,cv_px,sizeof expected);ht_snapshot_encode(snapshot,1234,0,0);ht_feed_t saved=ht_feed;float talk_saved=ht_talk_value;
   ui_init();assert(ht_snapshot_apply(snapshot,sizeof snapshot));ht_draw();assert(!memcmp(expected,cv_px,sizeof expected)&&!memcmp(&saved,&ht_feed,sizeof saved)&&ht_talk_value==talk_saved);
  }
  ht_snapshot_encode(snapshot,1234,0,0);
  const uint32_t bad_talk[]={0x7fc00000u,0x7f800000u,0xff800000u,0xbe800000u,0x3fa00000u};
  for(unsigned i=0;i<sizeof bad_talk/sizeof bad_talk[0];i++){
   for(unsigned j=0;j<4;j++)snapshot[160+j]=(uint8_t)(bad_talk[i]>>(j*8));
   assert(!ht_snapshot_apply(snapshot,sizeof snapshot));
  }
  ht_snapshot_encode(snapshot,1234,0,0);
  snapshot[0]^=1;assert(!ht_snapshot_apply(snapshot,sizeof snapshot));snapshot[0]^=1;assert(!ht_snapshot_apply(snapshot,sizeof snapshot-1));
  snapshot[32]=0;snapshot[33]=0;snapshot[34]=128;snapshot[35]=127;assert(!ht_snapshot_apply(snapshot,sizeof snapshot));
  lcd_sync();fprintf(stderr,"native angle/deformation/reward frames, visible eyes, caption-free rewards, snapshot/speech pixel parity, voice-mouth pause/Zen/approach, empty neutral boot/one-minute READY/GLO start, GLO reload/SAVE ignored, six physical keys, paused morph/zoom and DMA ownership pass\n");return 0;
 }
 if(argc>1&&!strcmp(argv[1],"--snapshot-fixture")){
  ht_feed_load();uint8_t raw[HT_SNAPSHOT_BYTES];ht_talk_value=argc>3?strtof(argv[3],0):0;ht_feed.served=argc>2?atoi(argv[2]):0;if(ht_feed.served){ht_feed.state=HT_FED;ht_feed.return_time=.8f;}ht_snapshot_encode(raw,1000,0,0);fwrite(raw,1,sizeof raw,stdout);return 0;
 }
 if(argc>1&&!strcmp(argv[1],"--snapshot-stream")){
  uint8_t raw[HT_SNAPSHOT_BYTES];
  while(fread(raw,1,sizeof raw,stdin)==sizeof raw){if(!ht_snapshot_apply(raw,sizeof raw))return 3;ht_draw();if(fwrite(cv_px,2,240*240,stdout)!=240*240)return 2;fflush(stdout);}lcd_sync();return 0;
 }
 if(argc>1&&!strcmp(argv[1],"--game-stream")){
  float y,p,m,t,dt;unsigned keys,reset,paused,face;
  while(scanf("%f %f %f %f %u %u %u %u %f",&y,&p,&m,&t,&keys,&reset,&paused,&face,&dt)==9){
   ht_motion=!paused;ht_test_mode=!face;
   host_keys=(keys&HT_LEFT?1u<<0:0)|(keys&HT_RIGHT?1u<<2:0)|(keys&HT_FORWARD?1u<<26:0)|(keys&HT_BACK?1u<<24:0)|(keys&HT_DOWN?1u<<1:0)|(keys&HT_UP?1u<<25:0);
   host_audio_advance((unsigned)(fm_clampf(dt,0,.1f)*44100));
   host_buttons=reset?1u<<B_GLO:0;host_ms+=(uint32_t)(fm_clampf(dt,0,.1f)*1000);ui_input_only();frame(y,p,m,t);
   if(fwrite(cv_px,2,240*240,stdout)!=240*240)return 2;fflush(stdout);
  }lcd_sync();return 0;
 }
 if(argc>1&&!strcmp(argv[1],"--stream")){
  float y,p,m,t;while(scanf("%f %f %f %f",&y,&p,&m,&t)==4){frame(y,p,m,t);if(fwrite(cv_px,2,240*240,stdout)!=240*240)return 2;fflush(stdout);}lcd_sync();return 0;
 }
 if(argc>1&&!strcmp(argv[1],"--reward-frame")){
  ht_feed.state=HT_FED;ht_feed.served=argc>3?atoi(argv[3]):1;ht_feed.return_time=argc>2?strtof(argv[2],0):.8f;
  ht_feed.reach=ht_feed.return_time<HT_DISSOLVE_SECONDS?1:0;ht_feed.pitch=-.65f;frame(0,0,.1f,0);
 }else if(argc>1&&!strcmp(argv[1],"--pour-frame")){
  ht_feed_load();int n=argc>2?atoi(argv[2]):0;
  for(int i=0;i<n;i++){
   float c=-ht_feed.roll*1.4f-ht_feed.roll_v*.55f+ht_feed.food_x*2.8f+ht_feed.vx*.7f;
   unsigned k=ht_feed.reach<.999f?HT_FORWARD:HT_DOWN;if(c>.025f)k|=HT_LEFT;else if(c<-.025f)k|=HT_RIGHT;ht_feed_step(k);
  }frame(0,0,.22f,(float)n*HT_DT);
 }else{
  if(argc>1)ht_feed_load();
  if(argc>10)ht_talk_value=fm_clampf(strtof(argv[10],0),0,1);
  if(argc>9)ht_feed.anger=ht_feed.frustration=fm_clampf(strtof(argv[9],0),0,1);
  if(argc>7)ht_feed.wait_time=fm_maxf(0,strtof(argv[7],0));if(argc>8)ht_feed.served=atoi(argv[8]);
  if(argc>5)ht_feed.reach=fm_clampf(strtof(argv[5],0),0,1);if(argc>6)ht_feed.roll=strtof(argv[6],0);
  frame(argc>1?strtof(argv[1],0):0,argc>2?strtof(argv[2],0):0,argc>3?strtof(argv[3],0):.22f,argc>4?strtof(argv[4],0):0);
 }
 fprintf(stdout,"P6\n240 240\n255\n");for(int i=0;i<240*240;i++){uint16_t c=swap16(cv_px[i]);uint8_t rgb[3]={(uint8_t)(((c>>11)&31)*255/31),(uint8_t)(((c>>5)&63)*255/63),(uint8_t)((c&31)*255/31)};fwrite(rgb,1,3,stdout);}lcd_sync();return 0;
}

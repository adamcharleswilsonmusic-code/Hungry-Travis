/* SPDX-License-Identifier: GPL-3.0-only
 * Hungry Travis: native shared-code spoon-balance test. No save writes. */
#include "ht_face.h"
#include "ht_spoon.h"
#include "ht_feed.h"
#include "ht_reward.h"
#include "../dsp/ht_audio.h"
static float ht_yaw=.12f,ht_pitch=0,ht_mouth_value=.22f,ht_time,ht_talk_value;
static uint32_t ht_last_ms,ht_old_buttons,ht_accum,ht_render_frames,ht_render_total_us,ht_render_max_us,ht_render_last_us;
static int ht_motion=1,ht_test_mode=1;
static void ht_center(int y,const char*s,uint16_t c){
 int x=(240-text_w(&FONT_HINT,s))/2;
 cv_text(x+1,y+1,&FONT_HINT,s,RGB(6,15,17));cv_text(x,y,&FONT_HINT,s,c);
}
static void ht_title(int y,const char*s,uint16_t c){
 int x=(240-text_w(&FONT_PLAY,s))/2;
 cv_text(x+1,y+2,&FONT_PLAY,s,RGB(6,15,17));cv_text(x,y,&FONT_PLAY,s,c);
}
void ui_init(void){ht_yaw=.12f;ht_pitch=0;ht_mouth_value=.22f;ht_time=0;ht_motion=1;ht_test_mode=1;ht_last_ms=plat_ms();ht_old_buttons=plat_buttons();ht_accum=0;ht_feed_init();ht_feed.state=HT_SPILLED;ht_talk_value=0;ht_audio_game_reset();}
void ui_input_only(void){
 uint32_t now=plat_ms(),elapsed=now-ht_last_ms;ht_last_ms=now;if(elapsed>100)elapsed=100;
 uint32_t b=plat_buttons(),fresh=b&~ht_old_buttons,k=plat_keys();ht_old_buttons=b;
 ht_yaw=fm_clampf(ht_yaw+(float)plat_enc(EN_SELECT)*.065f,-1.5f,1.5f);
 ht_mouth_value=fm_clampf(ht_mouth_value+(float)plat_enc(EN_K1)*.045f,0,1);
 ht_pitch=fm_clampf(ht_pitch+(float)plat_enc(EN_K2)*.035f,-.4f,.4f);
 if(fresh&(1u<<B_HOME)){ht_yaw=.12f;ht_pitch=0;ht_mouth_value=.22f;ht_feed_load();ht_accum=0;}
 if(fresh&(1u<<B_GLO)){ht_feed_load();ht_accum=0;}
 if(fresh&(1u<<B_SEL)){ht_test_mode=!ht_test_mode;ht_accum=0;}
 if(fresh&(1u<<B_PLAY)){ht_motion=!ht_motion;ht_accum=0;}
 uint32_t input=(k&(1u<<0)?HT_LEFT:0)|(k&(1u<<2)?HT_RIGHT:0)|(k&(1u<<26)?HT_FORWARD:0)|(k&(1u<<24)?HT_BACK:0)|(k&(1u<<1)?HT_DOWN:0)|(k&(1u<<25)?HT_UP:0);
 if(ht_motion){ht_time+=(float)elapsed*.001f;if(ht_test_mode){
  ht_accum+=elapsed*120u;while(ht_accum>=1000u){ht_feed_step(input);ht_accum-=1000u;}
 }}
 ht_audio_speech_window((ht_feed.state==HT_LOADED&&ht_feed.reach<.35f)||(ht_feed.state==HT_SPILLED&&ht_feed.spilled==0&&ht_feed.served==0));
 ht_audio_update(elapsed,input,!ht_motion,ht_test_mode,ht_feed.state,ht_feed.served,ht_feed.spilled,(fresh&((1u<<B_HOME)|(1u<<B_GLO)))!=0);
 /* Preserve the displayed speech pose while paused. Reward expressions own the
  * mouth during Zen/morph; collision remains the independent fixed aperture. */
 if(ht_motion)ht_talk_value=ht_test_mode&&ht_feed.state==HT_FED?0:fm_clampf(ht_audio_mouth(),0,1);
 plat_glow(1);plat_play_red(0);plat_leds((ht_motion?(1u<<B_PLAY):0)|(ht_test_mode&&ht_feed.state==HT_SPILLED?(1u<<B_GLO):0),(1u<<0)|(1u<<2)|(1u<<24)|(1u<<26)|(1u<<1)|(1u<<25));
}
/* A draw-only camera move: the same old head becomes the next physical bite.
 * No new simulation state, allocations, scene cache or second framebuffer. */
static void ht_cycle_draw(float zoom){
 float old_eye=ht_eye_roll,old_serenity=ht_serenity;
 /* The new face enters from outside the frame as the camera pulls back. */
 ht_model_identity();ht_model_scale=1+3.0f*(1-zoom);
 ht_model_y=9.0f*(1-zoom);ht_eye_roll=ht_serenity=0;ht_face_set_anger(0);
 ht_face_draw(ht_yaw,ht_pitch,.18f,ht_time);
 for(unsigned i=0;i<240u*240u;i++)ht_depth[i]=0;
 /* One rigid camera transform carries the ENTIRE next spoonful. At the
  * start its bite is the head-sized sphere and the giant bowl extends beyond
  * the frame; pulling back reveals bowl, handle and food at the same scale. */
 ht_vec3 b=ht_feed_bowl();
 ht_spoon_pose pose=ht_spoon_pose_make(b.x,b.y,b.z,0,.22f);
 ht_spoon_xyz target=ht_spoon_transform(.025f,.13f,0,b.x,b.y,b.z,0,.22f);
 float scale=1+(1.18f/.13f-1)*(1-zoom);
 ht_vec3 center={target.x*zoom,target.y+(.1f-target.y)*(1-zoom),target.z*zoom};
 ht_vec3 pan={center.x-scale*target.x,center.y-scale*target.y,center.z-scale*target.z};
 ht_model_identity();ht_model_scale=scale;ht_model_x=pan.x;ht_model_y=pan.y;ht_model_z=pan.z;
 /* A short reveal preserves continuity with the last uninterrupted morph
  * picture; this is already the giant spoon, never a small spoon under it. */
 ht_coverage=(int)(16*ht_smooth(zoom/.12f));
 ht_spoon_draw(b.x,b.y,b.z,0,.22f,0,0,0,0);ht_coverage=16;
 if(zoom>=.97f){
  /* Once only eight pixels in radius, use the ordinary bite itself so the next
   * loaded frame has the exact same geometry, pose, shading and color. */
  ht_spoon_bite(.025f,.13f,0,&pose);
 }else{
  /* Express that same rigid transform in the original portrait's larger
   * model units, retaining the exact morph mesh while the orb is visible. */
  ht_model_scale=scale*.13f/1.18f;
  ht_model_x=pan.x+scale*target.x;ht_model_y=pan.y+scale*target.y-.1f*ht_model_scale;ht_model_z=pan.z+scale*target.z;
  ht_skin=ht_spoon_food;ht_morph=1;ht_head(0);
 }
 ht_model_identity();
 ht_eye_roll=old_eye;ht_serenity=old_serenity;
}
static void ht_draw(void){
 cv_begin(240,240,RGB(23,36,34));ht_model_identity();
 int rewarding=ht_test_mode&&ht_feed.state==HT_FED;
 float morph=rewarding?ht_cycle_morph():0,zoom=rewarding?ht_cycle_zoom():0;
 float reward=rewarding?ht_reward_envelope(ht_feed.return_time):0;
 ht_coverage=16;ht_eye_roll=reward;ht_serenity=reward*(ht_reward_stage()/8.0f);
 if(rewarding)ht_reward_background(ht_feed.return_time);
 float mouth=ht_test_mode?fm_clampf(.18f+1.05f*ht_feed.reach,0,1):ht_mouth_value;
 if(ht_test_mode&&!rewarding)mouth=fm_maxf(mouth,.1f+.8f*ht_talk_value);
 if(rewarding)mouth=.1f;
 ht_face_set_anger(ht_test_mode?ht_feed_anger():0);ht_spoon_set_food(ht_feed.served);
 if(morph>=1){ht_cycle_draw(zoom);}else{
  ht_morph=morph;
  if(morph>0){
   /* One material steadily takes over the whole head as the details retract. */
   ht_skin.r+=(int)((ht_spoon_food.r-ht_skin.r)*morph);
   ht_skin.g+=(int)((ht_spoon_food.g-ht_skin.g)*morph);
   ht_skin.b+=(int)((ht_spoon_food.b-ht_skin.b)*morph);
  }
  ht_face_draw(ht_yaw,ht_pitch,mouth,ht_time);ht_model_identity();
  if(ht_test_mode){
   ht_vec3 b=ht_feed_bowl(),f=ht_feed_food_local();
   if(rewarding)ht_coverage=(int)(16*(1-fm_clampf(ht_feed.return_time/HT_DISSOLVE_SECONDS,0,1)));
   if(ht_coverage>0)ht_spoon_draw(b.x,b.y,b.z,ht_feed.roll,ht_feed.pitch,f.x,f.z,f.y,ht_feed.state==HT_LOADED||ht_feed.state==HT_FALLING);
   ht_coverage=16;
  }
 }
 ht_model_identity();
 /* Reward pictures have no captions. Pause remains visible in every mode. */
 int warning=ht_test_mode&&ht_feed_anger()>=.2f&&!rewarding;
 float phase=ht_feed.time;phase-=fm_floorf(phase/8.0f)*8.0f;
 if(!ht_motion){ht_title(3,"PAUSED",RGB(255,218,160));}
 else if(!rewarding){
  if(ht_test_mode&&ht_feed.state==HT_SPILLED){
   ht_title(0,ht_feed.spilled==0&&ht_feed.served==0?"HUNGRY TRAVIS":"WHOOPS!",RGB(255,174,132));ht_center(21,"PRESS GLO FOR NEW BEAN",RGB(245,223,180));
  }else if(warning&&phase<2.6f){
   ht_center(2,"TRAVIS IS GETTING ANGRY",RGB(247,202,163));
   ht_center(17,"YOU BETTER FEED HIM!",RGB(247,202,163));
  }else{ht_title(3,"HUNGRY TRAVIS",RGB(255,218,160));}
 }
 cv_blit(0,0);
}
void ui_frame(void){
 ui_input_only();
#ifndef OM_HOST
 uint32_t start=fm1_ticks();
#endif
 ht_draw();ht_render_frames++;
#ifndef OM_HOST
 ht_render_last_us=(fm1_ticks()-start)/FM1_TICKS_PER_US;ht_render_total_us+=ht_render_last_us;if(ht_render_last_us>ht_render_max_us)ht_render_max_us=ht_render_last_us;
#endif
}
int ui_dirty(void){return 1;}
void ui_say(const char*a,const char*b){cv_begin(240,240,RGB(19,30,29));ht_center(95,a,RGB(238,223,191));ht_center(120,b,RGB(176,202,181));cv_blit(0,0);}

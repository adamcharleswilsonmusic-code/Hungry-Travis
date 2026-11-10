/* SPDX-License-Identifier: GPL-3.0-only
 * Fixed-step spoon-balance prototype, shared by native firmware and host tests.
 * No renderer, heap, I/O or saved progress. Units are model-world coordinates. */
#pragma once
#include <stdint.h>
#include "../dsp/fastmath.h"
#define HT_DT (1.0f/120.0f)
#define HT_REWARD_SECONDS 1.8f
#define HT_DISSOLVE_SECONDS .45f
#define HT_CYCLE_FEEDS 5
#define HT_CYCLE_ZEN_SECONDS 3.3f
#define HT_CYCLE_MORPH_SECONDS .5f
#define HT_CYCLE_ZOOM_SECONDS 1.2f
#define HT_MAX_REWARD_SECONDS 5.0f
enum {HT_LOADED,HT_FALLING,HT_SPILLED,HT_FED};
enum {HT_LEFT=1,HT_RIGHT=2,HT_FORWARD=4,HT_BACK=8,HT_DOWN=16,HT_UP=32};
typedef struct{float x,y,z;}ht_vec3;
typedef struct{
 float time,reach,roll,roll_v,pitch,pitch_v,wait_time,anger,frustration,return_time,return_reach,return_roll,return_pitch;
 float food_x,food_z,food_y,vx,vz;
 ht_vec3 falling,fall_velocity;
 int state,served,spilled,pouring;
}ht_feed_t;
static ht_feed_t ht_feed;
/* Five feeds traverse the approved effect range, then a fresh character starts. */
static int ht_reward_stage(void){return ht_feed.served>0?((ht_feed.served-1)%HT_CYCLE_FEEDS)*2:0;}
/* Cycle state is derived entirely from the authoritative snapshot's existing fields. */
static int ht_cycle_reward(void){return ht_feed.served>0&&ht_feed.served%HT_CYCLE_FEEDS==0;}
/* Keep the first four rewards intact; the fifth reaches the next spoonful sooner. */
static float ht_reward_base_duration(void){return ht_cycle_reward()?HT_CYCLE_ZEN_SECONDS:HT_REWARD_SECONDS+.35f*ht_reward_stage();}
static float ht_reward_duration(void){return ht_reward_base_duration()+(ht_cycle_reward()?HT_CYCLE_MORPH_SECONDS+HT_CYCLE_ZOOM_SECONDS:0);}
static float ht_smooth(float x){x=fm_clampf(x,0,1);return x*x*(3-2*x);}
static float ht_cycle_morph(void){return ht_cycle_reward()?ht_smooth((ht_feed.return_time-ht_reward_base_duration())/HT_CYCLE_MORPH_SECONDS):0;}
static float ht_cycle_zoom(void){return ht_cycle_reward()?ht_smooth((ht_feed.return_time-ht_reward_base_duration()-HT_CYCLE_MORPH_SECONDS)/HT_CYCLE_ZOOM_SECONDS):0;}
static ht_vec3 ht_feed_bowl(void){return (ht_vec3){0,-1.10f+.63f*ht_feed.reach,1.45f-.16f*ht_feed.reach};}
/* Same transform as spoon geometry: local Rz, then Rx, then translation. */
static ht_vec3 ht_feed_world(float x,float y,float z){
 ht_vec3 b=ht_feed_bowl();float cr=fm_cosf(ht_feed.roll),sr=fm_sinf(ht_feed.roll),cp=fm_cosf(ht_feed.pitch),sp=fm_sinf(ht_feed.pitch);
 float xx=cr*x-sr*y,yy=sr*x+cr*y;
 return (ht_vec3){b.x+xx,b.y+cp*yy-sp*z,b.z+sp*yy+cp*z};
}
static ht_vec3 ht_feed_local(ht_vec3 p){
 ht_vec3 b=ht_feed_bowl();float cr=fm_cosf(ht_feed.roll),sr=fm_sinf(ht_feed.roll),cp=fm_cosf(ht_feed.pitch),sp=fm_sinf(ht_feed.pitch);
 float x=p.x-b.x,y=cp*(p.y-b.y)+sp*(p.z-b.z),z=-sp*(p.y-b.y)+cp*(p.z-b.z);
 return (ht_vec3){cr*x+sr*y,-sr*x+cr*y,z};
}
static void ht_feed_load(void){
 int served=ht_feed.served,spilled=ht_feed.spilled;float waiting=ht_feed.wait_time,anger=ht_feed.anger,frustration=ht_feed.frustration;ht_feed=(ht_feed_t){0};
 ht_feed.wait_time=waiting;ht_feed.anger=anger;ht_feed.frustration=frustration;
 ht_feed.served=served;ht_feed.spilled=spilled;ht_feed.pitch=.22f;ht_feed.food_y=.13f;
 ht_feed.food_x=.025f;ht_feed.vx=.035f;ht_feed.state=HT_LOADED;
}
static void ht_feed_init(void){ht_feed=(ht_feed_t){0};ht_feed_load();}
static float ht_feed_anger(void){return ht_feed.anger;}
static void ht_feed_miss(void){
 ht_feed_t*f=&ht_feed;f->state=HT_SPILLED;f->spilled++;f->frustration=fm_minf(1,f->frustration+.25f);
 /* A missed bite returns the empty spoon immediately. HT_SPILLED ignores
  * movement below, so it stays here until the player's explicit reload. */
 f->reach=f->roll=f->roll_v=f->pitch_v=0;f->pitch=.22f;f->pouring=0;
 f->return_time=f->return_reach=f->return_roll=0;f->return_pitch=.22f;
}
static void ht_feed_step(uint32_t keys){
 ht_feed_t*f=&ht_feed;f->time+=HT_DT;
 /* One frustration step per missed bite; smooth expression and color together. */
 float rate=f->anger>f->frustration?2.5f:.65f;
 f->anger+=fm_clampf(f->frustration-f->anger,-rate*HT_DT,rate*HT_DT);
 ht_vec3 before=ht_feed_world(f->food_x,f->food_y,f->food_z);
 if(f->state==HT_FED){
  f->return_time+=HT_DT;
  /* Dissolve at the delivery pose, then reset invisibly during the reward. */
  if(f->return_time>=HT_DISSOLVE_SECONDS){f->reach=0;f->roll=0;f->pitch=.22f;}
  if(f->return_time>=ht_reward_duration())ht_feed_load();return;
 }
 f->wait_time=fm_minf(60,f->wait_time+HT_DT);
 if(f->state==HT_SPILLED)return;
 float advance=(keys&HT_FORWARD?1.0f:0)-(keys&HT_BACK?1.0f:0);
 f->reach=fm_clampf(f->reach+advance*.18f*HT_DT,0,1);
 float steer=(keys&HT_LEFT?1.0f:0)-(keys&HT_RIGHT?1.0f:0);
 /* A gently unsteady hand makes balancing an active task. */
 float hand=.18f+.24f*fm_sinf(f->time*1.8f)+.13f*fm_sinf(f->time*4.3f);
 f->roll_v+=(steer*3.2f+hand-1.1f*f->roll-1.7f*f->roll_v)*HT_DT;
 f->roll=fm_clampf(f->roll+f->roll_v*HT_DT,-.85f,.85f);
 float tip=(keys&HT_UP?1.0f:0)-(keys&HT_DOWN?1.0f:0);
 f->pitch_v+=(tip*3.2f+.8f*(.22f-f->pitch)-1.8f*f->pitch_v)*HT_DT;
 f->pitch=fm_clampf(f->pitch+f->pitch_v*HT_DT,-1.0f,1.0f);
 if((f->pitch<=-1&&f->pitch_v<0)||(f->pitch>=1&&f->pitch_v>0))f->pitch_v=0;
 if(f->state==HT_FALLING){
  ht_vec3 previous=f->falling;
  f->fall_velocity.y-=2.4f*HT_DT;
  f->falling.x+=f->fall_velocity.x*HT_DT;f->falling.y+=f->fall_velocity.y*HT_DT;f->falling.z+=f->fall_velocity.z*HT_DT;
  /* A bite must leave the bowl and actually cross the mouth entrance. */
  if(previous.z>.86f&&f->falling.z<=.86f){
   float u=(previous.z-.86f)/(previous.z-f->falling.z);
   float x=previous.x+(f->falling.x-previous.x)*u;
   float y=previous.y+(f->falling.y-previous.y)*u;
   float aperture=x*x/(.32f*.32f)+(y+.56f)*(y+.56f)/(.23f*.23f);
   if(f->reach>.78f&&f->pouring&&aperture<1.0f){f->state=HT_FED;f->served++;f->wait_time=0;f->frustration=0;f->return_time=0;f->return_reach=f->reach;f->return_roll=f->roll;f->return_pitch=f->pitch;}
   else{ht_feed_miss();}
  }else if(f->falling.y< -2.2f||f->falling.z<.70f){ht_feed_miss();}
  return;
 }
 /* Gravity projected onto the bowl plus its concave restoring slope. */
 f->vx+=(-3.4f*fm_sinf(f->roll)-2.8f*f->food_x-.65f*f->vx+.08f*fm_sinf(f->time*3.1f))*HT_DT;
 f->vz+=(3.4f*fm_sinf(f->pitch)-4.5f*f->food_z-.9f*f->vz-advance*.16f)*HT_DT;
 f->food_x+=f->vx*HT_DT;f->food_z+=f->vz*HT_DT;
 float rim=f->food_x*f->food_x/(.29f*.29f)+f->food_z*f->food_z/(.33f*.33f);
 f->food_y=.13f+.11f*(f->food_x*f->food_x/(.32f*.32f)+f->food_z*f->food_z/(.36f*.36f));
 if(rim>1){
  f->falling=ht_feed_world(f->food_x,f->food_y,f->food_z);
  f->fall_velocity=(ht_vec3){(f->falling.x-before.x)/HT_DT,(f->falling.y-before.y)/HT_DT,(f->falling.z-before.z)/HT_DT};
  f->pouring=f->pitch<-.3f&&f->food_z<-.12f&&f->fall_velocity.z<-.04f;
  f->state=HT_FALLING;return;
 }
}
static ht_vec3 ht_feed_food_local(void){return ht_feed.state==HT_FALLING?ht_feed_local(ht_feed.falling):(ht_vec3){ht_feed.food_x,ht_feed.food_y,ht_feed.food_z};}

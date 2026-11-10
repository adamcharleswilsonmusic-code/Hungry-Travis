#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/app/ht_feed.h"
static unsigned balance(unsigned extra){
 float c=-ht_feed.roll*1.4f-ht_feed.roll_v*.55f+ht_feed.food_x*2.8f+ht_feed.vx*.7f;
 return extra|(c>.025f?HT_LEFT:c<-.025f?HT_RIGHT:0);
}
static void carry_to_mouth(void){
 for(int i=0;i<900&&ht_feed.reach<.999f&&ht_feed.state==HT_LOADED;i++)ht_feed_step(balance(HT_FORWARD));
 assert(ht_feed.reach>.99f&&ht_feed.state==HT_LOADED&&ht_feed.served==0);
}
static void finish_fall(void){for(int i=0;i<500&&ht_feed.state==HT_FALLING;i++)ht_feed_step(0);}
static void assert_empty_start(void){
 assert(ht_feed.state==HT_SPILLED&&ht_feed.reach==0&&ht_feed.roll==0&&ht_feed.roll_v==0&&ht_feed.pitch==.22f&&ht_feed.pitch_v==0&&!ht_feed.pouring);
 assert(ht_feed.return_time==0&&ht_feed.return_reach==0&&ht_feed.return_roll==0&&ht_feed.return_pitch==.22f);
}
int main(void){
 ht_feed_init();ht_feed.reach=.7f;ht_feed.roll=.41f;ht_feed.pitch=-.6f;
 ht_vec3 w=ht_feed_world(.11f,.15f,-.17f),p=ht_feed_local(w);
 assert(fm_fabsf(p.x-.11f)<.00001f&&fm_fabsf(p.y-.15f)<.00001f&&fm_fabsf(p.z+.17f)<.00001f);
 ht_feed_init();float pitch=ht_feed.pitch;for(int i=0;i<30;i++)ht_feed_step(HT_DOWN);assert(ht_feed.pitch<pitch);
 ht_feed_init();for(int i=0;i<30;i++)ht_feed_step(HT_UP);assert(ht_feed.pitch>pitch);
 /* A near, level bowl must not auto-feed, even while food stays balanced. */
 ht_feed_init();carry_to_mouth();for(int i=0;i<120;i++)ht_feed_step(balance(0));assert(ht_feed.state==HT_LOADED&&ht_feed.served==0);
 /* A real key-controlled pour crosses the mouth plane after leaving the bowl. */
 for(int i=0;i<600&&ht_feed.state==HT_LOADED;i++)ht_feed_step(balance(HT_DOWN));
 assert(ht_feed.state==HT_FALLING&&ht_feed.pouring&&ht_feed.spilled==0&&ht_feed.served==0);
 ht_vec3 start=ht_feed.falling;finish_fall();assert(ht_feed.state==HT_FED&&ht_feed.served==1&&ht_feed.spilled==0&&ht_feed.falling.z<start.z);
 int served=ht_feed.served;ht_feed_step(HT_DOWN);assert(ht_feed.served==served);ht_feed_load();assert(ht_feed.served==served&&ht_feed.state==HT_LOADED&&ht_feed.reach==0);
 /* Premature tipping misses below the mouth and counts exactly once. */
 ht_feed_init();for(int i=0;i<600&&ht_feed.state==HT_LOADED;i++)ht_feed_step(balance(HT_DOWN));assert(ht_feed.state==HT_FALLING);finish_fall();assert(ht_feed.state==HT_SPILLED&&ht_feed.spilled==1&&!ht_feed.served);
 for(int i=0;i<100;i++)ht_feed_step(HT_DOWN);assert(ht_feed.spilled==1);
 /* Side spills and tipping away cannot score. */
 ht_feed_init();for(int i=0;i<1200&&ht_feed.state==HT_LOADED;i++)ht_feed_step(HT_RIGHT|HT_FORWARD);assert(ht_feed.state==HT_FALLING);finish_fall();assert(ht_feed.state==HT_SPILLED&&!ht_feed.served);
 ht_feed_init();carry_to_mouth();for(int i=0;i<600&&ht_feed.state==HT_LOADED;i++)ht_feed_step(balance(HT_UP));assert(ht_feed.state==HT_FALLING&&!ht_feed.pouring);finish_fall();assert(ht_feed.state==HT_SPILLED&&!ht_feed.served);
 /* The wider opening accepts off-center and lower poured bites at approach. */
 ht_feed_init();ht_feed.reach=.8f;ht_feed.state=HT_FALLING;ht_feed.pouring=1;ht_feed.falling=(ht_vec3){.28f,-.56f,.87f};ht_feed.fall_velocity=(ht_vec3){0,0,-2};ht_feed_step(0);assert(ht_feed.state==HT_FED&&ht_feed.served==1);
 ht_feed_init();ht_feed.reach=.8f;ht_feed.state=HT_FALLING;ht_feed.pouring=1;ht_feed.falling=(ht_vec3){0,-.76f,.87f};ht_feed.fall_velocity=(ht_vec3){0,0,-2};ht_feed_step(0);assert(ht_feed.state==HT_FED&&ht_feed.served==1);
 /* Crossing the opening from a side spill still must not count as pouring. */
 ht_feed_init();ht_feed.reach=1;ht_feed.state=HT_FALLING;ht_feed.pouring=0;ht_feed.falling=(ht_vec3){0,-.56f,.87f};ht_feed.fall_velocity=(ht_vec3){0,0,-2};ht_feed_step(0);assert(ht_feed.state==HT_SPILLED&&!ht_feed.served);
 /* Crossing beside the opening hits the face, never a magnetic catch. */
 ht_feed_init();ht_feed.reach=1;ht_feed.state=HT_FALLING;ht_feed.pouring=1;ht_feed.falling=(ht_vec3){.4f,-.56f,.87f};ht_feed.fall_velocity=(ht_vec3){0,0,-2};ht_feed_step(0);assert(ht_feed.state==HT_SPILLED&&!ht_feed.served);
 /* A miss at the aperture or below the stage returns an EMPTY spoon,
  * clears its motion, and waits for Save despite every movement key. */
 for(int kind=0;kind<2;kind++){
  ht_feed_init();ht_feed.served=3;ht_feed.spilled=2;ht_feed.frustration=ht_feed.anger=.5f;
  ht_feed.reach=.92f;ht_feed.roll=.34f;ht_feed.roll_v=.7f;ht_feed.pitch=-.72f;ht_feed.pitch_v=-.4f;
  ht_feed.return_time=1;ht_feed.return_reach=.8f;ht_feed.return_roll=.2f;ht_feed.return_pitch=-.6f;
  ht_feed.state=HT_FALLING;ht_feed.pouring=1;
  ht_feed.falling=kind?(ht_vec3){.2f,-2.19f,1.0f}:(ht_vec3){.52f,-.56f,.87f};
  ht_feed.fall_velocity=kind?(ht_vec3){0,-2,0}:(ht_vec3){0,0,-2};
  ht_feed_step(HT_FORWARD|HT_LEFT|HT_UP);assert_empty_start();
  assert(ht_feed.served==3&&ht_feed.spilled==3&&ht_feed.frustration==.75f);
  const unsigned held[]={HT_LEFT,HT_RIGHT,HT_FORWARD,HT_BACK,HT_DOWN,HT_UP,HT_LEFT|HT_FORWARD|HT_DOWN,63};
  for(unsigned k=0;k<sizeof held/sizeof held[0];k++)for(int tick=0;tick<120;tick++){
   ht_feed_step(held[k]);assert_empty_start();
   assert(ht_feed.served==3&&ht_feed.spilled==3&&ht_feed.frustration==.75f);
  }
  /* This is the same reload function called by Save: add one fresh bean
   * without clearing anger, miss count or session feed progression. */
  float anger=ht_feed.anger;ht_feed_load();
  assert(ht_feed.state==HT_LOADED&&ht_feed.food_y==.13f&&ht_feed.food_x==.025f&&ht_feed.reach==0&&ht_feed.pitch==.22f);
  assert(ht_feed.served==3&&ht_feed.spilled==3&&ht_feed.frustration==.75f&&ht_feed.anger==anger);
 }
 /* Neutral at start; waiting alone does not produce an angry face. */
 ht_feed_init();ht_feed.state=HT_SPILLED;for(int i=0;i<1200;i++)ht_feed_step(0);assert(ht_feed.wait_time>9.9f&&ht_feed_anger()==0);
 /* Each actual miss adds one quarter, never once per spilled frame. Reload preserves it. */
 for(int miss=1;miss<=4;miss++){
  ht_feed_load();ht_feed.reach=1;ht_feed.state=HT_FALLING;ht_feed.pouring=1;ht_feed.falling=(ht_vec3){.5f,-.56f,.87f};ht_feed.fall_velocity=(ht_vec3){0,0,-2};ht_feed_step(0);
  assert(ht_feed.state==HT_SPILLED&&ht_feed.spilled==miss&&fm_fabsf(ht_feed.frustration-miss*.25f)<.00001f);
  for(int i=0;i<120;i++)ht_feed_step(0);assert(fm_fabsf(ht_feed_anger()-miss*.25f)<.00001f&&ht_feed.spilled==miss);
  float anger=ht_feed_anger();ht_feed_load();assert(ht_feed_anger()==anger&&ht_feed.frustration==anger);
 }
 ht_feed_miss();for(int i=0;i<120;i++)ht_feed_step(0);assert(ht_feed_anger()==1&&ht_feed.frustration==1);
 /* Successful delivery calms him during automatic withdrawal and new-color refill. */
 ht_feed_load();carry_to_mouth();for(int i=0;i<600&&ht_feed.state==HT_LOADED;i++)ht_feed_step(balance(HT_DOWN));finish_fall();assert(ht_feed.state==HT_FED&&ht_feed.wait_time==0&&ht_feed.frustration==0&&ht_feed.served==1);
 float old_reach=ht_feed.reach;for(int i=0;i<20;i++)ht_feed_step(0);assert(ht_feed.state==HT_FED&&ht_feed.reach==old_reach&&ht_feed_anger()<1);
 for(int i=0;i<40;i++)ht_feed_step(HT_DOWN|HT_FORWARD);assert(ht_feed.state==HT_FED&&ht_feed.reach==0&&ht_feed.served==1);
 for(int i=0;i<160;i++)ht_feed_step(0);assert(ht_feed.state==HT_LOADED&&ht_feed.reach==0&&ht_feed.served==1&&ht_feed.wait_time<.1f&&ht_feed_anger()==0);

 /* Each fresh character repeats five bounded rewards. Fifth performs the loop. */
 ht_feed_init();float previous=0;
 for(int n=1;n<=5;n++){ht_feed.served=n;float duration=ht_reward_duration();assert(duration>previous);previous=duration;assert(ht_cycle_reward()==(n==5));}
 assert(fm_fabsf(previous-HT_MAX_REWARD_SECONDS)<.00001f);
 ht_feed.served=6;assert(ht_reward_stage()==0&&ht_reward_duration()==HT_REWARD_SECONDS);
 ht_feed.served=1000;assert(ht_reward_stage()==8&&ht_reward_duration()<=HT_MAX_REWARD_SECONDS);
 /* Real fifth delivery enters the transition once, rejects held controls,
  * keeps its snapshot-compatible shape, then returns a neutral loaded spoon. */
 ht_feed_init();ht_feed.served=4;ht_feed.anger=.75f;ht_feed.reach=1;ht_feed.state=HT_FALLING;ht_feed.pouring=1;ht_feed.falling=(ht_vec3){0,-.56f,.87f};ht_feed.fall_velocity=(ht_vec3){0,0,-2};ht_feed_step(0);
 assert(ht_feed.state==HT_FED&&ht_feed.served==5&&ht_cycle_reward());
 float old_morph=0,old_zoom=0;int ticks=0;
 while(ht_feed.state==HT_FED){
  assert(ht_cycle_morph()>=old_morph&&ht_cycle_zoom()>=old_zoom);old_morph=ht_cycle_morph();old_zoom=ht_cycle_zoom();
  if(ht_cycle_zoom()>0)assert(ht_cycle_morph()==1);
  ht_feed_step(HT_FORWARD|HT_DOWN|HT_LEFT);ticks++;assert(ticks<605&&ht_feed.served==5);
 }
 assert(ticks>=600&&ht_feed.state==HT_LOADED&&ht_feed.reach==0&&ht_feed.anger==0&&ht_feed.frustration==0);
 assert(ht_cycle_morph()==0&&ht_cycle_zoom()==0&&sizeof(ht_feed_t)==112);
 /* Render-derived phases ignore input and are reproducible from saved state. */
 ht_feed.state=HT_FED;ht_feed.return_time=4.2f;ht_feed_t saved=ht_feed;
 for(int i=0;i<20;i++)ht_feed_step(0);ht_feed_t result=ht_feed;ht_feed=saved;
 for(int i=0;i<20;i++)ht_feed_step(HT_LEFT|HT_RIGHT|HT_FORWARD|HT_DOWN);
 assert(!memcmp(&result,&ht_feed,sizeof result));
 puts("six-key controls, physical pouring/misses, empty-spoon miss return/held-key lock/Save reload, bounded frustration, successful calming, deterministic five-feed morph/zoom/refill and unchanged 112-byte state pass");
}

/* SPDX-License-Identifier: GPL-3.0-only
 * One flash-streamed recorded voice and a quiet missed-bite cue. The main loop
 * publishes requests; only the audio callback owns the decoder. No heap. */
#include "omni.h"
#include "ht_audio.h"
#include "ht_adpcm.h"
#include "ht_voice_bank.h"
#include "ht_ambient.h"
volatile uint32_t om_active_keys,om_triggers,om_dropped;
volatile uint8_t om_seq_playing,om_seq_recording,om_seq_step;
volatile uint16_t om_seq_bpm=120;
volatile uint32_t om_seq_ticks,om_seq_mask[FMO_STEPS],om_seq_hit_dropped;
static const om_param_t ht_params[P_NPARAMS]={{"REVERB",0,127,0},{"DELAY",0,127,0},{"TIME",0,127,0},{"FEEDBACK",0,127,0}};
const om_param_t *om_param_info(int p){return ht_params+(p>=0&&p<P_NPARAMS?p:0);}
#define HT_AUDIO_BARRIER() __asm__ volatile("" ::: "memory")
#define HT_VOICE_FADE 220u
#define HT_DROP_LENGTH 5292u
#define HT_DROP_PEAK 10240u         /* +12.04dB from796; same soft120ms cue, unchanged voice/music gain. */
enum { HT_AUDIO_LOADED, HT_AUDIO_FALLING, HT_AUDIO_SPILLED, HT_AUDIO_FED };
/* Single aligned-word mailboxes: a newer event supersedes an unconsumed
 * request instead of building a speech backlog. All sample state is ISR-owned. */
static volatile uint32_t ht_voice_request,ht_drop_request,ht_reset_epoch,ht_paused;
static volatile uint32_t ht_reset_voice_cutoff,ht_reset_drop_cutoff;
static volatile uint32_t ht_voice_busy,ht_voice_priority,ht_mouth_q8;
static int ht_speech_window=1;
static uint32_t ht_request_serial,ht_seen_request,ht_seen_drop,ht_seen_reset;
static uint32_t ht_audio_frames,ht_voice_starts,ht_drop_starts,ht_clipped;
static volatile uint32_t ht_last_voice_id;
static ht_adpcm_t ht_decoder;
static int ht_clip=-1,ht_next_clip=-1;
static uint32_t ht_clip_priority,ht_next_priority,ht_voice_left,ht_voice_age,ht_switch_left;
static int32_t ht_pcm_a,ht_pcm_b;
static uint32_t ht_drop_left,ht_drop_phase;
static uint32_t ht_was_paused,ht_pause_left,ht_resume_left;
static int32_t ht_pause_sample,ht_last_mix;
typedef struct {
 uint32_t clock,last_reaction,due;
 int greeting,state,served,spilled,neutral_spilled,happy_next,angry_next,pending,priority;
} ht_audio_game_t;
static ht_audio_game_t ht_audio_game;
static void ht_request_voice(int clip,int priority){
 if(clip< -1||clip>=(int)HT_VOICE_COUNT)return;
 uint32_t serial=++ht_request_serial;HT_AUDIO_BARRIER();
 ht_voice_request=(serial<<10)|((uint32_t)(priority&3)<<8)|(uint32_t)(clip+1);
}
static void ht_pending_voice(int clip,int priority,uint32_t delay){
 ht_audio_game.pending=clip;ht_audio_game.priority=priority;ht_audio_game.due=ht_audio_game.clock+delay;
 ht_audio_game.greeting=0;
}
/* Selection belongs to the game loop; the callback publishes the last line
 * actually started. Avoid both that voice and the last selected reload, so a
 * superseded/canceled request cannot make the next GLO repeat its reaction. */
static int ht_reaction_clip(int first,int count,int index){
 uint32_t played=ht_last_voice_id;
 for(int tries=0;tries<count;tries++){
  int clip=first+index;
  if((uint32_t)clip!=played&&(uint32_t)(clip+1)!=ht_audio_game.last_reaction){
   ht_audio_game.last_reaction=(uint32_t)clip+1u;return clip;
  }
  if(++index==count)index=0;
 }
 return first; /* Three/five choices and only two exclusions: unreachable. */
}
static int ht_next_happy(void){
 int clip=ht_reaction_clip(HT_VOICE_HAPPY1,3,ht_audio_game.happy_next);
 ht_audio_game.happy_next=(clip-HT_VOICE_HAPPY1+1)%3;return clip;
}
static int ht_anger_clip(void){
 int misses=ht_audio_game.spilled-ht_audio_game.neutral_spilled;if(misses<1)misses=1;
 int stage=(misses-1)%5;
 int clip=ht_reaction_clip(HT_VOICE_ANGRY1,5,(stage+ht_audio_game.angry_next)%5);
 ht_audio_game.angry_next=(clip-HT_VOICE_ANGRY1+1+5-stage)%5;
 return clip;
}
void ht_audio_game_reset(void){
 ht_audio_game=(ht_audio_game_t){0};ht_audio_game.pending=HT_VOICE_HAPPY1;
 ht_audio_game.priority=1;ht_audio_game.happy_next=1;ht_audio_game.greeting=1;
 ht_audio_game.last_reaction=HT_VOICE_HAPPY1+1u;ht_paused=0;ht_speech_window=1;
 ht_reset_voice_cutoff=ht_voice_request;ht_reset_drop_cutoff=ht_drop_request;
 HT_AUDIO_BARRIER();ht_reset_epoch++;
}
void ht_audio_speech_window(int open){ht_speech_window=open!=0;}
void ht_audio_update(uint32_t dt,uint32_t keys,int paused,int feeding,int state,int served,int spilled,int reloaded){
 ht_paused=(uint32_t)(paused||!feeding);if(ht_paused)return;if(dt>500)dt=500;
 ht_audio_game_t*g=&ht_audio_game;g->clock+=dt;
 (void)keys;
 if(served>g->served){
  g->neutral_spilled=spilled;g->angry_next=0;
  int zen=HT_VOICE_ZEN1+(served-1)%5;
  g->last_reaction=(uint32_t)zen+1u;ht_pending_voice(zen,3,0);
 }else if(spilled>g->spilled){
  g->spilled=spilled;g->angry_next=0;ht_drop_request++;
  g->pending=-1;
  /* An empty spoon gets only its quiet cue. Stop any happy/angry sentence
   * with the usual short fade; the next reaction waits for a new bean. */
  ht_request_voice(-1,2);
 }else if(state==HT_AUDIO_LOADED&&g->state==HT_AUDIO_FED){
  /* A neutral refill waits for any long Zen recording, never cuts it off. */
  ht_pending_voice(ht_next_happy(),1,450);
 }else if(reloaded&&state==HT_AUDIO_LOADED){
  int angry=spilled>g->neutral_spilled;
  ht_pending_voice(angry?ht_anger_clip():ht_next_happy(),angry?2:1,100);
 }
 g->served=served;g->spilled=spilled;g->state=state;
 /* No idle chatter or delayed reaction once the spoon has left its fresh
  * starting area. In particular, a Zen-blocked refill line cannot start late
  * at his mouth. The startup greeting and Zen cues retain their own events. */
 int greeting=g->greeting&&g->pending==HT_VOICE_HAPPY1;
 if(g->pending>=0&&g->priority<3&&!greeting&&(!ht_speech_window||state!=HT_AUDIO_LOADED))g->pending=-1;
 if(g->pending>=0&&(int32_t)(g->clock-g->due)>=0&&
    (!ht_voice_busy||(uint32_t)g->priority>=ht_voice_priority||g->priority==3)){
  ht_request_voice(g->pending,g->priority);g->pending=-1;g->greeting=0;
 }
}
float ht_audio_mouth(void){
 /* Zen recordings are an inner voice even when their audio outlasts the
  * picture's reward and the next bean is already loaded. */
 uint32_t voice=ht_voice_busy;
 if(voice>=(uint32_t)HT_VOICE_ZEN1+1u&&voice<=HT_VOICE_COUNT)return 0;
 return (float)ht_mouth_q8*(1.0f/255.0f);
}
static void ht_voice_clear(void){
 ht_adpcm_stop(&ht_decoder);ht_clip=ht_next_clip=-1;ht_voice_left=ht_voice_age=ht_switch_left=0;
 ht_voice_busy=ht_voice_priority=ht_mouth_q8=0;
}
static void ht_voice_begin(int clip,uint32_t priority){
 ht_voice_clear();
 if(clip<0)return;
 if(clip>=(int)HT_VOICE_COUNT||ht_voice_clips[clip].sample_rate!=22050u||
    !ht_adpcm_start(&ht_decoder,&ht_voice_clips[clip])){om_dropped++;return;}
 ht_clip=clip;ht_clip_priority=priority;ht_voice_priority=priority;
 ht_voice_busy=(uint32_t)clip+1;ht_last_voice_id=(uint32_t)clip;
 ht_pcm_a=ht_adpcm_next(&ht_decoder);ht_pcm_b=ht_adpcm_next(&ht_decoder);
 ht_voice_left=ht_voice_clips[clip].sample_count*2u;ht_voice_starts++;om_triggers++;
}
void om_init(void){
 ht_ambient_init();
 ht_voice_clear();ht_request_serial=ht_seen_request=ht_seen_drop=ht_seen_reset=0;
 ht_voice_request=ht_drop_request=ht_reset_epoch=ht_paused=0;
 ht_reset_voice_cutoff=ht_reset_drop_cutoff=0;
 ht_audio_frames=ht_voice_starts=ht_drop_starts=ht_clipped=0;ht_last_voice_id=0xffffffffu;
 ht_drop_left=ht_drop_phase=0;om_active_keys=om_triggers=om_dropped=0;
 ht_was_paused=ht_pause_left=ht_resume_left=0;ht_pause_sample=ht_last_mix=0;
 ht_audio_game=(ht_audio_game_t){0};ht_audio_game.pending=-1;
}
static int32_t ht_voice_sample(void){
 if(ht_clip<0||!ht_voice_left)return 0;
 int32_t sample=ht_voice_age&1u?(ht_pcm_a+ht_pcm_b)/2:ht_pcm_a;
 uint32_t ramp=ht_voice_age<HT_VOICE_FADE?ht_voice_age:HT_VOICE_FADE;
 if(ht_voice_left<ramp)ramp=ht_voice_left;if(ht_switch_left&&ht_switch_left<ramp)ramp=ht_switch_left;
 sample=sample*(int32_t)ramp/(int32_t)HT_VOICE_FADE;sample=sample*3/4;
 if(ht_voice_age&1u){ht_pcm_a=ht_pcm_b;ht_pcm_b=ht_adpcm_next(&ht_decoder);}
 ht_voice_age++;ht_voice_left--;
 uint32_t target=ht_voice_envelope_at((unsigned)ht_clip,ht_voice_age/2u);
 if(target>ht_mouth_q8)ht_mouth_q8+=(target-ht_mouth_q8+63u)/64u;
 else ht_mouth_q8-=(ht_mouth_q8-target+255u)/256u;
 if(ht_switch_left&&!--ht_switch_left){int next=ht_next_clip;uint32_t priority=ht_next_priority;ht_voice_begin(next,priority);}
 else if(!ht_voice_left){
  if(ht_next_clip>=0){int next=ht_next_clip;uint32_t priority=ht_next_priority;ht_voice_begin(next,priority);}
  else ht_voice_clear();
 }return sample;
}
static int32_t ht_drop_sample(void){
 if(!ht_drop_left)return 0;uint32_t age=HT_DROP_LENGTH-ht_drop_left;
 uint32_t frequency=150u+130u*ht_drop_left/HT_DROP_LENGTH;
 ht_drop_phase+=frequency*97392u;
 int32_t triangle=(int32_t)(ht_drop_phase>>16);
 triangle=triangle<32768?triangle*2-32768:98303-triangle*2;
 uint32_t env=ht_drop_left*HT_DROP_PEAK/HT_DROP_LENGTH;if(age<220u)env=env*age/220u;ht_drop_left--;
 return triangle*(int32_t)env/32768;
}
void om_render(int32_t*out,uint32_t n,uint32_t gain){
 uint32_t epoch=ht_reset_epoch;HT_AUDIO_BARRIER();
 if(ht_seen_reset!=epoch){
  ht_voice_clear();ht_drop_left=0;ht_seen_reset=epoch;
  ht_seen_request=ht_reset_voice_cutoff;ht_seen_drop=ht_reset_drop_cutoff;
  ht_last_mix=ht_pause_sample=0;ht_pause_left=ht_resume_left=0;
 }
 uint32_t paused=ht_paused;
 if(paused&&!ht_was_paused){ht_pause_sample=ht_last_mix;ht_pause_left=HT_VOICE_FADE;}
 else if(!paused&&ht_was_paused)ht_resume_left=HT_VOICE_FADE;
 ht_was_paused=paused;
 if(!paused){
  uint32_t request=ht_voice_request;HT_AUDIO_BARRIER();
  if(request&&request!=ht_seen_request){
   ht_seen_request=request;int clip=(int)(request&255u)-1;uint32_t priority=(request>>8)&3u;
   if(ht_clip<0)ht_voice_begin(clip,priority);
   else if(priority>=ht_clip_priority){ht_next_clip=clip;ht_next_priority=priority;ht_switch_left=HT_VOICE_FADE;}
  }
  if(ht_seen_drop!=ht_drop_request){ht_seen_drop=ht_drop_request;ht_drop_left=HT_DROP_LENGTH;ht_drop_phase=0;ht_drop_starts++;}
 }
 if(gain>4096u)gain=4096u;
 for(uint32_t i=0;i<n;i++){
  int32_t sample;
  if(paused){sample=ht_pause_left?ht_pause_sample*(int32_t)ht_pause_left/(int32_t)HT_VOICE_FADE:0;if(ht_pause_left)ht_pause_left--;}
  else{
   sample=ht_voice_sample()+ht_drop_sample()+ht_ambient_next(ht_voice_busy!=0);
   if(ht_resume_left){sample=sample*(int32_t)(HT_VOICE_FADE-ht_resume_left)/(int32_t)HT_VOICE_FADE;ht_resume_left--;}
   ht_last_mix=sample;
  }
  if(sample>32760){sample=32760;ht_clipped++;}else if(sample< -32760){sample=-32760;ht_clipped++;}
  sample=sample*(int32_t)gain/4096;out[i*2u]=out[i*2u+1]=sample*256;
 }
 om_active_keys=(ht_voice_busy?1u:0u)|(ht_drop_left?2u:0u);ht_audio_frames+=n;
}
uint32_t ht_audio_debug(unsigned i){
 switch(i){case 0:return ht_audio_frames;case 1:return ht_voice_busy;case 2:return ht_voice_starts;
 case 3:return ht_drop_starts;case 4:return ht_voice_age/2u;case 5:return om_dropped;
 case 6:return ht_last_voice_id;case 7:return ht_clipped;case 8:return ht_mouth_q8;
 case 9:return ht_request_serial;case 10:return ht_paused;case 11:return (uint32_t)(ht_audio_game.pending+1);default:return 0;}
}
void om_set(int p,int v){(void)p;(void)v;}void om_trigger(int k,int v){(void)k;(void)v;}
void om_panic(void){ht_reset_voice_cutoff=ht_voice_request;ht_reset_drop_cutoff=ht_drop_request;HT_AUDIO_BARRIER();ht_reset_epoch++;}void game_sound(int e){if(e==S_HIT)ht_drop_request++;}
void game_voice(int c){ht_request_voice(c,3);}void game_music(int e){(void)e;}
void om_seq_toggle_play(void){}void om_seq_toggle_record(void){}void om_seq_clear(void){}void om_seq_set_tempo(int b){(void)b;}
int om_take_seq_hit(int*k){(void)k;return 0;}const char*om_sample_name(int k){(void)k;return "TRAVIS";}int om_placeholder(void){return 0;}
uint32_t om_audio_stat(int i){return ht_audio_debug((unsigned)i);}uint32_t om_music_beat(void){return 0;}
void game_music_restart(void){}void game_music_variant(int z){(void)z;}void game_music_victory(void){}void game_music_silence(void){}void game_alarm_stop(void){}
void game_engine(int r){(void)r;}void game_jet(int t){(void)t;}int game_voice_available(int c){return c>=0&&c<HT_VOICE_COUNT;}void game_voice_end_combat(void){}

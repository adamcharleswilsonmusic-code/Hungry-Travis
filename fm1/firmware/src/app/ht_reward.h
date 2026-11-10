/* SPDX-License-Identifier: GPL-3.0-only
 * Success celebration: outward rainbow ribbons behind the native 3D portrait.
 * No extra framebuffer, heap, strobing, or changes to the feeding collision. */
#pragma once
static float ht_reward_envelope(float t){
 float in=fm_clampf(t/.22f,0,1),out=fm_clampf((ht_reward_base_duration()+(ht_cycle_reward()?HT_CYCLE_MORPH_SECONDS*.6f:0)-t)/.55f,0,1);
 return in*out;
}
static void ht_reward_background(float t){
 static const uint8_t colors[6][3]={{65,236,235},{179,105,255},{255,94,180},{255,189,58},{136,244,95},{98,149,255}};
 float envelope=ht_reward_envelope(t);int ray,segment,stage=ht_reward_stage(),rays=24+4*stage;
 if(envelope<=0)return;
 /* Curved streams drift outward, with a single smooth fade at either end. */
 for(ray=0;ray<rays;ray++){
  float base=ray*(FM_TWO_PI/rays)+t*(.38f+.025f*stage);
  int oldx=0,oldy=0;
  for(segment=0;segment<=14;segment++){
   float r=58+segment*13.0f;
   float angle=base+(.14f+.018f*stage)*fm_sinf(r*.035f-t*3.5f+ray*.7f);
   int x=120+(int)(fm_cosf(angle)*r),y=150+(int)(fm_sinf(angle)*r);
   float glow=envelope*(.52f+.48f*fm_sinf(r*.035f-t*5+ray)*fm_sinf(r*.035f-t*5+ray));
   const uint8_t*c=colors[(ray+segment/5+stage)%6];
   uint16_t color=RGB((int)(23+(c[0]-23)*glow),(int)(36+(c[1]-36)*glow),(int)(34+(c[2]-34)*glow));
   if(segment){cv_line(oldx,oldy,x,y,color);cv_line(oldx+1,oldy,x+1,y,color);cv_line(oldx,oldy+1,x,y+1,color);}
   oldx=x;oldy=y;
  }
 }

 /* Later bites add mandala rings and outward-orbiting sparks behind the head. */
 if(stage>=2){
  int ring,k;
  for(ring=0;ring<stage-1;ring++){
   int ox=0,oy=0;float radius=79+ring*6.0f;
   const uint8_t*c=colors[(ring+stage)%6];float glow=envelope*.72f;
   uint16_t color=RGB((int)(23+(c[0]-23)*glow),(int)(36+(c[1]-36)*glow),(int)(34+(c[2]-34)*glow));
   for(k=0;k<=64;k++){
    float a=k*(FM_TWO_PI/64),r=radius+(3+stage)*fm_sinf(a*(5+ring%3)+t*.8f);
    int x=120+(int)(r*fm_cosf(a)),y=150+(int)(r*fm_sinf(a));
    if(k)cv_line(ox,oy,x,y,color);ox=x;oy=y;
   }
  }
  for(k=0;k<stage*4;k++){
   float phase=t*.24f+k*.137f;phase-=fm_floorf(phase);
   float radius=77+phase*98,a=k*2.39996f+t*.33f;
   int x=120+(int)(radius*fm_cosf(a)),y=150+(int)(radius*fm_sinf(a));
   const uint8_t*c=colors[k%6];float glow=envelope*(1-phase);
   uint16_t color=RGB((int)(23+(c[0]-23)*glow),(int)(36+(c[1]-36)*glow),(int)(34+(c[2]-34)*glow));
   cv_line(x-2,y,x+2,y,color);cv_line(x,y-2,x,y+2,color);
  }
 }
}

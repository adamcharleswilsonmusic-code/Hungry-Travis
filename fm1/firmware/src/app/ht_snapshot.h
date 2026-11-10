/* SPDX-License-Identifier: GPL-3.0-only
 * Complete render inputs, explicitly little-endian, versioned and host-validated.
 * Carries authoritative device state, never replayed controls or new physics. */
#pragma once
#define HT_SNAPSHOT_REVISION 797u
#define HT_SNAPSHOT_BYTES 164u
_Static_assert(sizeof(float)==4&&sizeof(ht_feed_t)==112,"snapshot layout");
static uint32_t ht_snapshot_hash(const uint8_t*p,unsigned n){uint32_t h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static uint32_t ht_float_bits(float f){union{float f;uint32_t u;}v;v.f=f;return v.u;}
static float ht_bits_float(uint32_t u){union{float f;uint32_t u;}v;v.u=u;return v.f;}
static void ht_snapshot_encode(uint8_t*out,uint32_t uptime,uint32_t xruns,uint32_t clipped){
 uint32_t words[41]={0x48545331u,HT_SNAPSHOT_REVISION,uptime,ht_render_frames,xruns,clipped,(uint32_t)ht_test_mode,(uint32_t)ht_motion,
 ht_float_bits(ht_yaw),ht_float_bits(ht_pitch),ht_float_bits(ht_mouth_value),ht_float_bits(ht_time)};
 memcpy(words+12,&ht_feed,sizeof(ht_feed));words[40]=ht_float_bits(ht_talk_value);
 for(unsigned i=0;i<41;i++)for(unsigned j=0;j<4;j++)out[i*4+j]=(uint8_t)(words[i]>>(j*8));
}
#ifdef OM_HOST
static int ht_snapshot_apply(const uint8_t*raw,unsigned size){
 uint32_t w[41];if(size!=HT_SNAPSHOT_BYTES)return 0;
 for(unsigned i=0;i<41;i++){w[i]=0;for(unsigned j=0;j<4;j++)w[i]|=(uint32_t)raw[i*4+j]<<(j*8);}
 if(w[0]!=0x48545331u||w[1]!=HT_SNAPSHOT_REVISION||w[6]>1||w[7]>1||w[36]>HT_FED||w[39]>1||w[37]>0x7fffffffu||w[38]>0x7fffffffu)return 0;
 for(unsigned i=8;i<36;i++)if((w[i]&0x7f800000u)==0x7f800000u||fm_fabsf(ht_bits_float(w[i]))>((i==11||i==12)?100000000.0f:1000.0f))return 0;
 float yaw=ht_bits_float(w[8]),pitch=ht_bits_float(w[9]),mouth=ht_bits_float(w[10]),talk=ht_bits_float(w[40]);
 if((w[40]&0x7f800000u)==0x7f800000u||talk<0||talk>1)return 0;
 ht_feed_t f;memcpy(&f,w+12,sizeof f);
 if(yaw< -1.5f||yaw>1.5f||pitch<-.4f||pitch>.4f||mouth<0||mouth>1||f.reach<0||f.reach>1||f.anger<0||f.anger>1||f.frustration<0||f.frustration>1||f.return_time<0||f.return_time>HT_MAX_REWARD_SECONDS+.02f||fm_fabsf(f.roll)>.86f||fm_fabsf(f.pitch)>1.01f)return 0;
 ht_feed=f;ht_yaw=yaw;ht_pitch=pitch;ht_mouth_value=mouth;ht_talk_value=talk;ht_time=ht_bits_float(w[11]);ht_test_mode=(int)w[6];ht_motion=(int)w[7];return 1;
}
#endif

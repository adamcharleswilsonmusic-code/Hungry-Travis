/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dsp/ht_adpcm.h"
#include "ht_voice_bank.h"

static void invalid_descriptors(void)
{
    uint8_t data[512]={0};
    ht_adpcm_t s;
    ht_voice_clip_t c={data,256,505,22050};
    assert(!ht_adpcm_start(&s,0) && s.error && !s.active);
    c.data=0; assert(!ht_adpcm_start(&s,&c)); c.data=data;
    uint32_t badbytes[]={0,1,255,257,393472,UINT32_MAX};
    for(unsigned i=0;i<sizeof badbytes/sizeof *badbytes;i++) {
        c.encoded_bytes=badbytes[i]; assert(!ht_adpcm_start(&s,&c)); assert(ht_adpcm_next(&s)==0);
    }
    c.encoded_bytes=256; c.sample_count=0; assert(!ht_adpcm_start(&s,&c));
    c.sample_count=506; assert(!ht_adpcm_start(&s,&c));
    c.sample_count=UINT32_MAX; assert(!ht_adpcm_start(&s,&c));
    c.sample_count=505; c.sample_rate=0; assert(!ht_adpcm_start(&s,&c));
    c.sample_rate=48001; assert(!ht_adpcm_start(&s,&c));
    c.sample_rate=22050;
    data[2]=89; assert(ht_adpcm_start(&s,&c)); assert(ht_adpcm_next(&s)==0 && s.error && !s.active);
    data[2]=0; data[3]=1; assert(ht_adpcm_start(&s,&c)); assert(ht_adpcm_next(&s)==0 && s.error);
    data[3]=0; c.sample_count=1; data[0]=0;data[1]=128;
    assert(ht_adpcm_start(&s,&c)); assert(ht_adpcm_next(&s)==-32768 && !s.active && s.position==1);
    for(int i=0;i<1000;i++) assert(ht_adpcm_next(&s)==0);
    data[0]=255;data[1]=127; assert(ht_adpcm_start(&s,&c)); assert(ht_adpcm_next(&s)==32767);
    c.sample_count=506;c.encoded_bytes=512;data[258]=255;
    assert(ht_adpcm_start(&s,&c));
    for(int i=0;i<505;i++)ht_adpcm_next(&s);
    assert(s.active && !s.error && s.position==505);
    assert(ht_adpcm_next(&s)==0 && s.error && !s.active);
    memset(data,255,sizeof data);data[2]=88;data[3]=0;c.sample_count=505;c.encoded_bytes=256;
    assert(ht_adpcm_start(&s,&c));while(s.active)ht_adpcm_next(&s);assert(!s.error);
    memset(data,0,sizeof data);data[0]=255;data[1]=127;data[2]=88;
    memset(data+4,0x77,252);
    assert(ht_adpcm_start(&s,&c));while(s.active)assert(ht_adpcm_next(&s)==32767);assert(!s.error);
}

static void randomized_bounds(void)
{
    uint8_t data[2048]; uint32_t seed=19;
    for(unsigned trial=0;trial<10000;trial++) {
        for(unsigned i=0;i<sizeof data;i++){seed=seed*1664525u+1013904223u;data[i]=(uint8_t)(seed>>24);}
        uint32_t blocks=1+(seed%8), bytes=blocks*256;
        for(unsigned o=0;o<bytes;o+=256){data[o+2]%=89;data[o+3]=0;}
        uint32_t count=(blocks-1)*505+1+(seed%505);
        ht_voice_clip_t c={data,bytes,count,22050};ht_adpcm_t s;
        assert(ht_adpcm_start(&s,&c));
        for(unsigned i=0;i<count;i++)ht_adpcm_next(&s);
        assert(!s.active && !s.error && s.position==count);
        assert(!ht_adpcm_next(&s));
    }
}
int main(int argc,char **argv)
{
    assert(sizeof(ht_adpcm_t)<=48);
    invalid_descriptors(); randomized_bounds();
    uint32_t aggregate=0;
    for(unsigned clip=0;clip<HT_VOICE_COUNT;clip++) {
        ht_adpcm_t s; const ht_voice_clip_t *c=ht_voice_clips+clip;
        assert(ht_adpcm_start(&s,c));
        FILE *f=0;
        if(argc>1){char path[4096];snprintf(path,sizeof path,"%s/voice-%u.pcm",argv[1],clip);f=fopen(path,"wb");assert(f);}
        for(unsigned i=0;i<c->sample_count;i++) {
            assert(s.active && s.position==i);
            int16_t v=ht_adpcm_next(&s);
            if(f){uint8_t b[2]={(uint8_t)v,(uint8_t)((uint16_t)v>>8)};assert(fwrite(b,1,2,f)==2);}
        }
        if(f)fclose(f);
        assert(!s.active && !s.error && s.position==c->sample_count);
        assert(ht_adpcm_next(&s)==0);
        assert(ht_voice_envelope_at(clip,c->sample_count)==0);
        assert(ht_voice_envelope_at(clip,0)==ht_voice_envelopes[clip].data[0]);
        assert(ht_adpcm_start(&s,c));ht_adpcm_stop(&s);assert(!s.active && ht_adpcm_next(&s)==0);
        aggregate+=c->sample_count;
    }
    assert(ht_voice_envelope_at(HT_VOICE_COUNT,UINT32_MAX)==0);
    printf("PASS IMA streaming:13 clips,%u samples; malformed blocks,10,000 randomized bounds,exact ends; state%zuB\n",aggregate,sizeof(ht_adpcm_t));
    return 0;
}

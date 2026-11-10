/* SPDX-License-Identifier: GPL-3.0-only */
/* Pure native generator checks and optional60-second WAV preview.
 * Host elapsed time is not an FM-1 CPU measurement. No audio hardware is used.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "dsp/ht_ambient.h"

#if HT_AMBIENT_ENABLE
static uint64_t hash_next(uint64_t hash,int32_t value)
{
    hash^=(uint16_t)value;return hash*1099511628211ull;
}
static uint64_t render_hash(unsigned seconds,int speech)
{
    uint64_t h=1469598103934665603ull;
    for(unsigned i=0;i<44100u*seconds;i++)h=hash_next(h,ht_ambient_next(speech));
    return h;
}
static void le16(FILE*f,uint32_t v){fputc((int)(v&255u),f);fputc((int)((v>>8)&255u),f);}
static void le32(FILE*f,uint32_t v){le16(f,v);le16(f,v>>16);}
static void preview(const char*path)
{
    FILE*f=fopen(path,"wb");assert(f);
    unsigned count=60u*44100u,bytes=count*2u;
    fwrite("RIFF",1,4,f);le32(f,36+bytes);fwrite("WAVEfmt ",1,8,f);le32(f,16);
    le16(f,1);le16(f,1);le32(f,44100);le32(f,88200);le16(f,2);le16(f,16);
    fwrite("data",1,4,f);le32(f,bytes);ht_ambient_init();
    for(unsigned i=0;i<count;i++) {
        /*10s normal,10s duck,10s normal,10s duck,20s normal. No voice added. */
        unsigned seconds=i/44100u;int speech=(seconds>=10&&seconds<20)||(seconds>=30&&seconds<40);
        le16(f,(uint16_t)ht_ambient_next(speech));
    }
    assert(!ferror(f));assert(!fclose(f));
}
#endif
int main(int argc,char**argv)
{
#if HT_AMBIENT_ENABLE
    assert(sizeof(ht_ambient_phase)+sizeof(ht_ambient_slow_phase)+sizeof(ht_ambient_step)+
           sizeof(ht_ambient_amp)+sizeof(ht_ambient_control)+sizeof(ht_ambient_age)+sizeof(ht_ambient_gain)+
           sizeof(ht_ambient_melody_index)+sizeof(ht_ambient_melody_left)+sizeof(ht_ambient_melody_step)<=96);
    ht_ambient_init();assert(ht_ambient_next(0)==0);
    for(unsigned i=1;i<44100u;i++) {
        int32_t v=ht_ambient_next(0);assert(v>=-1152&&v<=1152);
        if(i<441u)assert(v>=-12&&v<=12); /* Quiet first10ms of the1s start fade. */
    }
    ht_ambient_init();uint64_t first=render_hash(12,0);
    ht_ambient_init();assert(render_hash(12,0)==first);
    /* No progression between calls is intrinsic: reset/replay prefixes produce
     * the identical next sample, so callback pause adds no hidden time source. */
    ht_ambient_init();render_hash(2,0);int32_t after_pause=ht_ambient_next(0);
    ht_ambient_init();render_hash(2,0);assert(ht_ambient_next(0)==after_pause);
    ht_ambient_init();render_hash(2,0);
    uint32_t oldgain=ht_ambient_gain;
    for(unsigned i=0;i<4410u;i++) {
        int32_t v=ht_ambient_next(1);assert(v>=-1152&&v<=1152);
        assert(ht_ambient_gain<=oldgain&&oldgain-ht_ambient_gain<=11u);oldgain=ht_ambient_gain;
    }
    assert(ht_ambient_gain==24576u);
    for(unsigned i=0;i<44100u;i++){int32_t v=ht_ambient_next(1);assert(v>=-432&&v<=432);}
    /* Compare identical oscillator positions with and without speech: ducking
     * preserves the bed and phase rather than muting or retriggering it. */
    int32_t normal[2048];unsigned duck_nonzero=0;
    ht_ambient_init();render_hash(2,0);
    for(unsigned i=0;i<4410u;i++)ht_ambient_next(0);
    for(unsigned i=0;i<2048u;i++)normal[i]=ht_ambient_next(0);
    ht_ambient_init();render_hash(2,0);
    for(unsigned i=0;i<4410u;i++)ht_ambient_next(1);
    for(unsigned i=0;i<2048u;i++) {
        int32_t v=ht_ambient_next(1);assert(v==normal[i]*3/8);
        if(v)duck_nonzero++;
    }
    assert(duck_nonzero>1900u);
    oldgain=ht_ambient_gain;
    for(unsigned i=0;i<13654u;i++) {
        ht_ambient_next(0);assert(ht_ambient_gain>=oldgain&&ht_ambient_gain-oldgain<=3u);oldgain=ht_ambient_gain;
    }
    assert(ht_ambient_gain==65536u);
    /* Exercise a complete phrase: every note is reached before the next
     * change, with phase-continuous pitch slew and fixed low anchors. */
    ht_ambient_init();int32_t last_step=ht_ambient_melody_step;
    uint32_t visited=0;
    for(unsigned i=0;i<58u*44100u;i++) {
        ht_ambient_next(0);
        int32_t d=ht_ambient_melody_step-last_step;
        assert(d>=-HT_AMBIENT_MELODY_SLEW&&d<=HT_AMBIENT_MELODY_SLEW);
        if(ht_ambient_melody_step==(int32_t)ht_ambient_melody_steps[ht_ambient_melody_index])
            visited|=1u<<ht_ambient_melody_index;
        last_step=ht_ambient_melody_step;
    }
    assert(visited==((1u<<HT_AMBIENT_MELODY_COUNT)-1u)&&ht_ambient_melody_index==0u);
    /* Long run exercises every unsigned phase wrap and slow-modulation state. */
    ht_ambient_init();render_hash(1,0);
    int64_t sum=0;uint64_t squares=0;unsigned peak=0;int32_t prior=ht_ambient_next(0);unsigned max_delta=0;
    const unsigned count=120u*44100u;
    clock_t started=clock();
    for(unsigned i=0;i<count;i++) {
        int32_t v=ht_ambient_next(0);assert(v>=-1152&&v<=1152);
        unsigned magnitude=(unsigned)(v<0?-v:v);if(magnitude>peak)peak=magnitude;
        unsigned delta=(unsigned)(v>prior?v-prior:prior-v);if(delta>max_delta)max_delta=delta;prior=v;
        sum+=v;squares+=(uint64_t)(v*v);
    }
    assert(sum>(int64_t)count*-1 && sum<(int64_t)count); /* DC magnitude<1 PCM16. */
    assert(peak>100&&squares/count>1000u&&max_delta<110u);
    double host_seconds=(double)(clock()-started)/CLOCKS_PER_SEC;
    printf("PASS ambient:deterministic,fixed state<=96B,slow melody,start fade,duck/restore,no DC,bounded continuous output; peak%u,max delta%u,mean%.6f,mean square%.1f\n",
           peak,max_delta,(double)sum/count,(double)squares/count);
    printf("Host-only timing:generated120s in%.3fs; no FM-1 performance claim\n",host_seconds);
    if(argc>1){preview(argv[1]);printf("Native preview:%s (60s mono PCM16,44.1kHz;duck10-20s,30-40s)\n",argv[1]);}
#else
    (void)argc;(void)argv;ht_ambient_init();
    for(unsigned i=0;i<44100;i++)assert(ht_ambient_next((int)(i&1u))==0);
    puts("PASS ambient disabled:API remains silent");
#endif
    return 0;
}

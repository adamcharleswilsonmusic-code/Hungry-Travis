/* SPDX-License-Identifier: GPL-3.0-only */
/* Behavioral host tests: no audio device, heap, UI mocks or firmware mutation.
 * Compile this unit alone with firmware/src and build/gen include paths. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Isolate speech behavior; ambient is verified separately. */
#define HT_AMBIENT_ENABLE 0
#include "../firmware/src/dsp/omni.c"

static int32_t output[512];
static void render_gain(unsigned frames,uint32_t gain)
{
    assert(frames<=256);
    om_render(output,frames,gain);
    for(unsigned i=0;i<frames;i++) {
        assert(output[2*i]==output[2*i+1]);
        assert(output[2*i]>=-32760*256 && output[2*i]<=32760*256);
    }
    assert(ht_audio_debug(8)<=255);
    assert(ht_audio_mouth()>=0 && ht_audio_mouth()<=1);
}
static void render(unsigned frames){render_gain(frames,4096);}
static void frames(unsigned count)
{
    while(count){unsigned n=count>256?256:count;render(n);count-=n;}
}
static void drain(void)
{
    unsigned blocks=0;
    while(ht_audio_debug(1)){render(256);assert(++blocks<2000);}
    assert(ht_audio_debug(5)==0 && ht_audio_debug(7)==0);
}
static void update(uint32_t dt,uint32_t keys,int pause,int state,int served,int spilled,int reload)
{
    ht_audio_update(dt,keys,pause,1,state,served,spilled,reload);
}
static void fresh(void)
{
    om_init();ht_audio_game_reset();update(0,0,0,HT_AUDIO_LOADED,0,0,0);render(256);
    assert(ht_audio_debug(2)==1 && ht_audio_debug(6)==HT_VOICE_HAPPY1);
    drain();
}
static void miss(int total,int served)
{
    update(1,1,0,HT_AUDIO_SPILLED,served,total,0);
    update(180,0,0,HT_AUDIO_SPILLED,served,total,0);render(256);
}
static void new_bean(int served,int spilled)
{
    update(1,1,0,HT_AUDIO_LOADED,served,spilled,1);
    update(500,0,0,HT_AUDIO_LOADED,served,spilled,0);render(256);
}
static void startup_and_cycles(void)
{
    fresh();
    for(int i=1;i<=7;i++) {
        unsigned before=ht_audio_debug(2);
        miss(i,0);assert(ht_audio_debug(2)==before && ht_audio_debug(1)==0);
        assert(ht_audio_debug(3)==(uint32_t)i);
        /* Waiting beside the empty spoon must never queue an angry recording. */
        for(int tick=0;tick<40;tick++){update(500,0,0,HT_AUDIO_SPILLED,0,i,0);render(256);}
        assert(ht_audio_debug(2)==before && ht_audio_debug(3)==(uint32_t)i);
        new_bean(0,i);
        assert(ht_audio_debug(6)==(uint32_t)(HT_VOICE_ANGRY1+(i-1)%5));drain();
    }
    update(1,0,0,HT_AUDIO_FED,1,7,0);render(256);
    assert(ht_audio_debug(6)==HT_VOICE_ZEN1);drain();
    miss(8,1);assert(ht_audio_debug(1)==0);new_bean(1,8);
    assert(ht_audio_debug(6)==HT_VOICE_ANGRY1);drain();
    fresh();
    for(int i=1;i<=10;i++) {
        update(1,0,0,HT_AUDIO_FED,i,0,0);render(256);
        assert(ht_audio_debug(6)==(uint32_t)(HT_VOICE_ZEN1+(i-1)%5));drain();
        /* Automatic happy refill must wait for its 450ms delay. */
        update(1,0,0,HT_AUDIO_LOADED,i,0,0);render(256);
        unsigned before=ht_audio_debug(2);
        update(449,0,0,HT_AUDIO_LOADED,i,0,0);render(256);assert(ht_audio_debug(2)==before);
        update(1,0,0,HT_AUDIO_LOADED,i,0,0);render(256);
        assert(ht_audio_debug(6)==(uint32_t)(HT_VOICE_HAPPY1+i%3));drain();
    }
}
static void no_idle_lines_and_explicit_refills(void)
{
    fresh();
    for(int state=HT_AUDIO_LOADED;state<=HT_AUDIO_SPILLED;state++) {
        for(int tick=0;tick<120;tick++){update(500,0,0,state,0,0,0);render(256);}
        assert(ht_audio_debug(2)==1 && ht_audio_debug(1)==0);
    }
    /* A real new-bean event may speak immediately after startup finishes. */
    fresh();new_bean(0,0);
    assert(ht_audio_debug(2)==2 && ht_audio_debug(6)==HT_VOICE_HAPPY2);
    drain();
    /* Held input without a fresh Save/new-bean edge cannot repeat speech. */
    for(int tick=0;tick<120;tick++){update(500,1,0,HT_AUDIO_LOADED,0,0,0);render(256);}
    assert(ht_audio_debug(2)==2);new_bean(0,0);
    assert(ht_audio_debug(2)==3 && ht_audio_debug(6)==HT_VOICE_HAPPY3);drain();
}
static void neutral_three_line_rotation_and_late_greeting(void)
{
    fresh();unsigned previous=HT_VOICE_HAPPY1;
    for(unsigned i=1;i<=9;i++) {
        new_bean(0,0);unsigned clip=ht_audio_debug(6);
        assert(clip==HT_VOICE_HAPPY1+i%3 && clip!=previous);
        previous=clip;drain();
    }
    /* Recurring happy1 is an ordinary GLO reaction, never a second startup
     * exemption that can begin late after the spoon window has closed. */
    fresh();new_bean(0,0);drain();new_bean(0,0);drain();
    unsigned starts=ht_audio_debug(2);
    update(1,0,0,HT_AUDIO_LOADED,0,0,1);
    assert(ht_audio_debug(11)==HT_VOICE_HAPPY1+1u);
    ht_audio_speech_window(0);update(500,0,0,HT_AUDIO_LOADED,0,0,0);render(256);
    assert(ht_audio_debug(11)==0 && ht_audio_debug(2)==starts && !ht_audio_debug(1));
    /* Canceled selections advance the requests, but the chooser must still
     * avoid the last line actually spoken (including the boot greeting). */
    fresh();ht_audio_speech_window(0);
    for(unsigned i=0;i<2;i++){update(1,0,0,HT_AUDIO_LOADED,0,0,1);update(500,0,0,HT_AUDIO_LOADED,0,0,0);render(1);}
    assert(ht_audio_debug(2)==1);
    ht_audio_speech_window(1);new_bean(0,0);
    assert(ht_audio_debug(6)==HT_VOICE_HAPPY2);drain();
}
static void repeated_angry_refills_do_not_repeat(void)
{
    fresh();miss(1,0);unsigned previous=UINT32_MAX;
    for(unsigned i=0;i<12;i++) {
        new_bean(0,1);unsigned clip=ht_audio_debug(6);
        assert(clip==HT_VOICE_ANGRY1+i%5 && clip!=previous);
        previous=clip;drain();
    }
    /* A new stage still starts at its escalating line unless that would
     * repeat the one just heard during an extra GLO at the previous stage. */
    fresh();miss(1,0);new_bean(0,1);drain();new_bean(0,1);drain();
    assert(ht_audio_debug(6)==HT_VOICE_ANGRY2);
    miss(2,0);new_bean(0,2);assert(ht_audio_debug(6)==HT_VOICE_ANGRY3);drain();
    /* A calming Zen line is intervening speech: the next fresh frustration
     * cycle can start at angry1 again rather than retaining an old exclusion. */
    fresh();miss(1,0);new_bean(0,1);drain();
    update(1,0,0,HT_AUDIO_FED,1,1,0);render(256);drain();
    miss(2,1);new_bean(1,2);assert(ht_audio_debug(6)==HT_VOICE_ANGRY1);drain();
    /* Two GLO events before the ISR consumes a request choose different
     * lines; only the newest request starts, without building a backlog. */
    fresh();miss(1,0);new_bean(0,1);drain();
    unsigned starts=ht_audio_debug(2);
    update(1,0,0,HT_AUDIO_LOADED,0,1,1);update(500,0,0,HT_AUDIO_LOADED,0,1,0);
    unsigned first=ht_voice_request&255u;assert(first==HT_VOICE_ANGRY2+1u);
    update(1,0,0,HT_AUDIO_LOADED,0,1,1);update(500,0,0,HT_AUDIO_LOADED,0,1,0);
    assert((ht_voice_request&255u)==HT_VOICE_ANGRY3+1u && (ht_voice_request&255u)!=first);
    render(1);assert(ht_audio_debug(2)==starts+1u && ht_audio_debug(6)==HT_VOICE_ANGRY3);drain();
}
static void miss_cancels_nonzen_without_losing_sfx(void)
{
    fresh();new_bean(0,0);frames(2000);
    assert(ht_audio_debug(1)==HT_VOICE_HAPPY2+1);
    unsigned starts=ht_audio_debug(2);
    miss(1,0); /* Bounded fade must have completed within this256-frame callback. */
    assert(ht_audio_debug(1)==0 && ht_audio_debug(2)==starts && ht_audio_debug(3)==1);
    assert(ht_audio_debug(8)==0);
    frames(6000);
    for(unsigned i=0;i<512;i++)assert(output[i]==0);
    for(int tick=0;tick<40;tick++){update(500,0,0,HT_AUDIO_SPILLED,0,1,0);render(256);}
    assert(ht_audio_debug(1)==0 && ht_audio_debug(2)==starts && ht_audio_debug(3)==1);
    /* A miss also cancels a requested recording that the ISR has not consumed. */
    fresh();update(1,1,0,HT_AUDIO_LOADED,0,0,1);
    update(500,0,0,HT_AUDIO_LOADED,0,0,0); /* Request is ready, no audio callback yet. */
    starts=ht_audio_debug(2);
    miss(1,0);assert(ht_audio_debug(1)==0 && ht_audio_debug(2)==starts && ht_audio_debug(3)==1);
    new_bean(0,1);assert(ht_audio_debug(6)==HT_VOICE_ANGRY1 && ht_audio_debug(2)==starts+1);
}
static void zen_survives_neutral_refill(void)
{
    fresh();update(1,0,0,HT_AUDIO_FED,1,0,0);render(256);frames(44100);
    assert(ht_audio_debug(6)==HT_VOICE_ZEN1);
    update(500,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    update(450,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    assert(ht_audio_debug(6)==HT_VOICE_ZEN1 && ht_audio_debug(2)==2);
    drain();update(0,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    assert(ht_audio_debug(6)==HT_VOICE_HAPPY2 && ht_audio_debug(2)==3);
}
static void approaching_mouth_cancels_zen_blocked_refill_line(void)
{
    fresh();update(1,0,0,HT_AUDIO_FED,1,0,0);render(256);frames(44100);
    update(500,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    update(450,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    assert(ht_audio_debug(6)==HT_VOICE_ZEN1 && ht_audio_debug(2)==2);
    assert(ht_audio_debug(11)!=0); /* Refill line is due, waiting behind Zen. */
    ht_audio_speech_window(0); /* UI: loaded spoon has approached the mouth. */
    update(0,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    assert(ht_audio_debug(11)==0 && ht_audio_debug(6)==HT_VOICE_ZEN1);
    drain();
    for(int tick=0;tick<120;tick++){update(500,0,0,HT_AUDIO_LOADED,1,0,0);render(256);}
    assert(ht_audio_debug(2)==2 && ht_audio_debug(1)==0 && ht_audio_debug(8)==0);
    /* Returning to the starting area cannot resurrect the cancelled line. */
    ht_audio_speech_window(1);update(500,0,0,HT_AUDIO_LOADED,1,0,0);render(256);
    assert(ht_audio_debug(2)==2 && ht_audio_debug(11)==0);
    /* Zen remains immediate even with the speech window closed. */
    ht_audio_speech_window(0);update(1,0,0,HT_AUDIO_FED,2,0,0);render(256);
    assert(ht_audio_debug(2)==3 && ht_audio_debug(6)==HT_VOICE_ZEN2);
}
static void explicit_save_replaces_same_priority_line(void)
{
    fresh();new_bean(0,0);frames(2000);
    assert(ht_audio_debug(6)==HT_VOICE_HAPPY2);
    unsigned starts=ht_audio_debug(2);
    update(1,1,0,HT_AUDIO_LOADED,0,0,1);
    update(100,0,0,HT_AUDIO_LOADED,0,0,0);
    render(219);assert(ht_audio_debug(2)==starts && ht_audio_debug(6)==HT_VOICE_HAPPY2);
    render(1);assert(ht_audio_debug(2)==starts+1 && ht_audio_debug(6)==HT_VOICE_HAPPY3);
    assert(abs(output[0])<=32767*256/220);
    render(1);assert(output[0]==0);drain();
    update(500,0,0,HT_AUDIO_LOADED,0,0,0);render(256);
    assert(ht_audio_debug(2)==starts+1 && ht_audio_debug(11)==0);
}
static void zen_inner_voice_never_moves_mouth(void)
{
    for(unsigned id=HT_VOICE_ZEN1;id<HT_VOICE_COUNT;id++) {
        fresh();
        int served=(int)(id-HT_VOICE_ZEN1)+1;
        update(1,0,0,HT_AUDIO_FED,served,0,0);render(1);
        assert(ht_audio_debug(6)==id && ht_audio_mouth()==0);
        unsigned total=ht_voice_clips[id].sample_count*2u;
        int heard=0,heard_after_refill=0,internal_envelope=0;
        for(unsigned frame=1;frame<total;frame++) {
            /* First reward finishes well before these complete recordings. */
            if(frame==79380u)update(1,0,0,HT_AUDIO_LOADED,served,0,0);
            render(1);
            assert(ht_audio_mouth()==0);
            if(output[0]){heard=1;if(frame>=79380u)heard_after_refill=1;}
            if(ht_audio_debug(8))internal_envelope=1;
        }
        assert(heard && heard_after_refill && internal_envelope);
        assert(ht_audio_debug(1)==0 && ht_audio_debug(2)==2);
    }
    /* Actual spoken happy/angry clips must retain their mouth animation. */
    for(unsigned id=HT_VOICE_HAPPY1;id<HT_VOICE_ZEN1;id++) {
        om_init();game_voice((int)id);render(1);
        int mouth_moved=0,heard=0;
        while(ht_audio_debug(1)) {
            render(256);
            if(ht_audio_mouth()>0)mouth_moved=1;
            for(unsigned i=0;i<512;i++)if(output[i])heard=1;
        }
        assert(mouth_moved && heard);
    }
}
static void bounded_preemption(void)
{
    fresh();miss(1,0);new_bean(0,1);frames(6000); /* Let the independent missed-bite tone end. */
    unsigned starts=ht_audio_debug(2);
    update(1,0,0,HT_AUDIO_FED,1,1,0);
    render(219);assert(ht_audio_debug(2)==starts && ht_audio_debug(6)==HT_VOICE_ANGRY1);
    render(1);assert(ht_audio_debug(2)==starts+1 && ht_audio_debug(6)==HT_VOICE_ZEN1);
    assert(abs(output[0])<=32767*256/220);
    render(1);assert(output[0]==0); /* New clip attack starts at zero. */
}
static void pause_freezes_and_fades(void)
{
    fresh();game_voice(HT_VOICE_HAPPY1);render(1);
    int32_t last=0;
    for(int i=0;i<30000;i++){render(1);last=output[0];if(abs(last)>3000*256)break;}
    assert(abs(last)>3000*256);
    unsigned pos=ht_audio_debug(4),mouth=ht_audio_debug(8),starts=ht_audio_debug(2);
    update(0,0,1,HT_AUDIO_LOADED,0,0,0);
    render(1);assert(abs(output[0]-last)<=abs(last)/100+256);
    render(219);render(256);
    for(unsigned i=0;i<512;i++)assert(output[i]==0);
    assert(ht_audio_debug(4)==pos && ht_audio_debug(8)==mouth && ht_audio_debug(2)==starts);
    update(0,0,0,HT_AUDIO_LOADED,0,0,0);render(1);
    assert(abs(output[0])<=32767*256/220);
    frames(220);assert(ht_audio_debug(4)>pos);
    /* The non-feeding view uses the same pause semantics. */
    ht_audio_update(0,0,0,0,HT_AUDIO_LOADED,0,0,0);frames(512);
    pos=ht_audio_debug(4);frames(1000);assert(ht_audio_debug(4)==pos);
}
static void reset_discards_stale_mailboxes(void)
{
    om_init();game_voice(HT_VOICE_ANGRY1);game_sound(S_HIT);om_panic();render(256);
    assert(ht_audio_debug(1)==0 && ht_audio_debug(2)==0 && ht_audio_debug(3)==0);
    for(unsigned i=0;i<512;i++)assert(output[i]==0);
    game_voice(HT_VOICE_ZEN1);render(1);assert(ht_audio_debug(6)==HT_VOICE_ZEN1);
    om_init();game_voice(HT_VOICE_ANGRY1);game_sound(S_HIT);ht_audio_game_reset();
    update(0,0,0,HT_AUDIO_LOADED,0,0,0);render(1);
    assert(ht_audio_debug(2)==1 && ht_audio_debug(6)==HT_VOICE_HAPPY1 && ht_audio_debug(3)==0);
}
static void gain_and_full_clip_duration(void)
{
    int32_t reference[512];
    om_init();game_voice(HT_VOICE_ANGRY3);frames(1024);render_gain(256,4096);memcpy(reference,output,sizeof output);
    om_init();game_voice(HT_VOICE_ANGRY3);frames(1024);render_gain(256,UINT32_MAX);assert(!memcmp(reference,output,sizeof output));
    render_gain(256,0);for(unsigned i=0;i<512;i++)assert(!output[i]);
    for(unsigned id=0;id<HT_VOICE_COUNT;id++) {
        om_init();game_voice((int)id);render(1);
        assert(ht_audio_debug(1)==id+1);
        frames(ht_voice_clips[id].sample_count*2u-2u);assert(ht_audio_debug(1)==id+1);
        render(1);assert(ht_audio_debug(1)==0);
        assert(ht_audio_debug(2)==1 && ht_audio_debug(5)==0 && ht_audio_debug(7)==0);
    }
}
static void voiced_outputs_remain_unchanged(void)
{
    /* Golden hashes captured from FM-1_794's unchanged standalone voice mixer
     * before raising the ambient/missed-bite gains. Hash normalized PCM16 so
     * the reference is independent of host integer byte order. */
    static const uint64_t expected[13]={
        192872519256183465ull,1257467539691706224ull,9532061885399486429ull,
        11492467397184378802ull,6415264979682318658ull,16991420167468416767ull,
        4254421625858266017ull,14692667228296250860ull,5169360371115146839ull,
        11076541617306879591ull,13689554711651304887ull,14383380684210599764ull,
        14306815401292661194ull
    };
    for(unsigned id=0;id<HT_VOICE_COUNT;id++) {
        om_init();game_voice((int)id);uint64_t hash=1469598103934665603ull;
        for(unsigned frame=0;frame<ht_voice_clips[id].sample_count*2u;frame++) {
            render(1);hash^=(uint16_t)(output[0]/256);hash*=1099511628211ull;
        }
        assert(hash==expected[id]);
    }
}
static void missed_bite_level_stays_subtle(void)
{
    om_init();game_sound(S_HIT);
    unsigned peak=0,max_delta=0;uint64_t squares=0;int32_t previous=0;
    for(unsigned i=0;i<HT_DROP_LENGTH;i++) {
        render(1);int32_t sample=output[0]/256;
        unsigned magnitude=(unsigned)abs(sample);if(magnitude>peak)peak=magnitude;
        unsigned delta=(unsigned)abs(sample-previous);if(delta>max_delta)max_delta=delta;previous=sample;
        squares+=(uint64_t)(sample*sample);
        assert(ht_audio_debug(2)==0 && ht_audio_mouth()==0);
    }
    assert(peak>9600u&&peak<=10240u&&max_delta<420u);
    double mean_square=(double)squares/HT_DROP_LENGTH;
    /* Original1024 cue:mean square106718.568. Tenfold level is100x energy. */
    assert(mean_square>10600000.0&&mean_square<10800000.0);
    render(1);assert(!output[0]&&ht_audio_debug(3)==1&&ht_audio_debug(7)==0);
    printf("Missed-bite cue:peak%u PCM16,max delta%u,mean square%.3f;voice output hashes unchanged\n",peak,max_delta,mean_square);
}
int main(void)
{
    startup_and_cycles();no_idle_lines_and_explicit_refills();neutral_three_line_rotation_and_late_greeting();
    repeated_angry_refills_do_not_repeat();miss_cancels_nonzen_without_losing_sfx();
    zen_survives_neutral_refill();approaching_mouth_cancels_zen_blocked_refill_line();
    explicit_save_replaces_same_priority_line();zen_inner_voice_never_moves_mouth();bounded_preemption();
    pause_freezes_and_fades();reset_discards_stale_mailboxes();gain_and_full_clip_duration();
    voiced_outputs_remain_unchanged();missed_bite_level_stays_subtle();
    puts("PASS Hungry Travis scheduler/mixer:startup,new-bean-only happy/angry,no idle speech,miss cancels talk/SFX once,Zen cycles,priority fades,pause freeze,reset mailboxes,gain bounds,all13 exact durations");
    return 0;
}

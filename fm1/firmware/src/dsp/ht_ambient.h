/* SPDX-License-Identifier: GPL-3.0-only */
/* Hungry Travis: a quiet continuous pad generated on the FM-1.
 * Four fixed-point voices, no beat, percussion, samples, heap or audio I/O.
 * Call at44100Hz. Not calling freezes phase, fade and duck state for pause.
 * Returned samples are actual mono PCM16; the mixer handles S24 conversion.
 * The mathematical output bound is1152 PCM16; speech gain limits it to432.
 */
#pragma once
#include <stdint.h>
#ifndef HT_AMBIENT_ENABLE
#define HT_AMBIENT_ENABLE 1
#endif

#if HT_AMBIENT_ENABLE
#include "felucca_tables.h"

static uint32_t ht_ambient_phase[4],ht_ambient_slow_phase[4];
static uint32_t ht_ambient_step[4],ht_ambient_amp[4];
static uint32_t ht_ambient_control,ht_ambient_age,ht_ambient_gain;
static uint32_t ht_ambient_melody_index,ht_ambient_melody_left;
static int32_t ht_ambient_melody_step;
static const uint32_t ht_ambient_base_step[4]={7150117u,10713070u,12740059u,28600467u};
/* D2,A2,C3 stay beneath a soft D4,E4,F4,A4,G4,E4,D4,C4,A3,C4,E4,D4 line.
 * Its higher register separates the melody from the warm low pad. Uneven
 *3–7second holds and gradual phase-continuous glides add movement
 * without retriggering an amplitude envelope or creating a rhythmic pulse.
 * Slow amplitude drift periods~77,53,37,30seconds;
 * pitch drift is in quadrature, under1.7cents either side of each note. */
static const uint32_t ht_ambient_slow_step[4]={81030u,118428u,168293u,205691u};
#define HT_AMBIENT_MELODY_COUNT 12u
#define HT_AMBIENT_MELODY_SLEW 12288
static const uint32_t ht_ambient_melody_steps[HT_AMBIENT_MELODY_COUNT]={28600467u,32102938u,34011878u,42852281u,38177043u,32102938u,28600467u,25480119u,21426141u,25480119u,32102938u,28600467u};
static const uint32_t ht_ambient_melody_lengths[HT_AMBIENT_MELODY_COUNT]={5u*44100u,3u*44100u,4u*44100u,6u*44100u,4u*44100u,5u*44100u,4u*44100u,6u*44100u,5u*44100u,3u*44100u,5u*44100u,7u*44100u};

static inline int32_t ht_ambient_sine(uint32_t phase)
{
    uint32_t i=phase>>22,frac=(phase>>6)&65535u;
    int32_t a=SINE[i],delta=(int32_t)SINE[i+1u]-a;
    return a+delta*(int32_t)frac/65536;
}
static inline void ht_ambient_init(void)
{
    static const uint32_t starts[4]={0u,0x29712683u,0x81234657u,0xc159d237u};
    static const uint32_t slow_starts[4]={0u,0x51627a31u,0x98345217u,0xda735921u};
    for(unsigned i=0;i<4;i++) {
        ht_ambient_phase[i]=starts[i];ht_ambient_slow_phase[i]=slow_starts[i];
        ht_ambient_step[i]=ht_ambient_base_step[i];ht_ambient_amp[i]=24576u;
    }
    ht_ambient_control=ht_ambient_age=0;ht_ambient_gain=65536u;
    ht_ambient_melody_index=0;ht_ambient_melody_left=ht_ambient_melody_lengths[0];
    ht_ambient_melody_step=(int32_t)ht_ambient_melody_steps[0];
}
static inline int32_t ht_ambient_next(int speech_active)
{
    int32_t mix=0;
    if(!ht_ambient_control) {
        int32_t difference=(int32_t)ht_ambient_melody_steps[ht_ambient_melody_index]-ht_ambient_melody_step;
        ht_ambient_melody_step+=difference>HT_AMBIENT_MELODY_SLEW?HT_AMBIENT_MELODY_SLEW:
            difference< -HT_AMBIENT_MELODY_SLEW?-HT_AMBIENT_MELODY_SLEW:difference;
        for(unsigned i=0;i<4;i++) {
            uint32_t p=ht_ambient_slow_phase[i];
            uint32_t base=i==3u?(uint32_t)ht_ambient_melody_step:ht_ambient_base_step[i];
            ht_ambient_amp[i]=(uint32_t)(24576+ht_ambient_sine(p)/4);
            int32_t drift=(int32_t)(base>>10)*ht_ambient_sine(p+0x40000000u)/32768;
            ht_ambient_step[i]=(uint32_t)((int32_t)base+drift);
            ht_ambient_slow_phase[i]+=ht_ambient_slow_step[i];
        }
    }
    if(!--ht_ambient_melody_left) {
        ht_ambient_melody_index++;
        if(ht_ambient_melody_index==HT_AMBIENT_MELODY_COUNT)ht_ambient_melody_index=0;
        ht_ambient_melody_left=ht_ambient_melody_lengths[ht_ambient_melody_index];
    }
    ht_ambient_control=(ht_ambient_control+1u)&63u;
    for(unsigned i=0;i<4;i++) {
        uint32_t p=ht_ambient_phase[i];
        int32_t wave=ht_ambient_sine(p)+ht_ambient_sine(p*2u)/8;
        mix+=wave*(int32_t)ht_ambient_amp[i]/32768;
        ht_ambient_phase[i]+=ht_ambient_step[i];
    }
    mix/=128; /* +3.01dB from795's181 divisor; still below-29dBFS peak. */
    /* Linear envelope changes are bounded per sample, never retrigger notes.
     * Speech attack~85ms, restore~310ms; no dependence on recorded voice data. */
    uint32_t target=speech_active?24576u:65536u;
    if(ht_ambient_gain>target) {
        uint32_t difference=ht_ambient_gain-target;
        ht_ambient_gain-=difference>11u?11u:difference;
    } else if(ht_ambient_gain<target) {
        uint32_t difference=target-ht_ambient_gain;
        ht_ambient_gain+=difference>3u?3u:difference;
    }
    mix=mix*(int32_t)ht_ambient_gain/65536;
    if(ht_ambient_age<44100u) {
        mix=mix*(int32_t)ht_ambient_age/44100;
        ht_ambient_age++;
    }
    return mix;
}
#else
static inline void ht_ambient_init(void){}
static inline int32_t ht_ambient_next(int speech_active){(void)speech_active;return 0;}
#endif

/* SPDX-License-Identifier: GPL-3.0-only
 * Optional authoritative render snapshots. CC119/channel16 leases viewing for
 * three seconds. No game controls or flash writes. OTA always owns USB SysEx.
 * Small immutable messages are drained in bounded USB-ISR batches. */
#pragma once
#include "ht_snapshot.h"
static uint8_t ht_capture_message[208];
static volatile uint32_t ht_capture_length,ht_capture_offset;
static volatile uint8_t ht_capture_enabled,ht_capture_sending;
static uint32_t ht_capture_lease,ht_capture_frame;
static uint8_t ht_diagnostics_pending;
static void ht_capture_stop(void){
 uint8_t active=ht_capture_enabled;ht_capture_enabled=ht_capture_sending=0;
 if(active){mo_r=mo_w;if(usb.config){midi_out_q[mo_w%MQ]=0x0000f705u;RING_PUBLISH();mo_w++;}}
}
static void capture_poll(void){
 uint32_t p,now=plat_ms();static uint32_t last_diagnostic;
 while(plat_midi_in(&p))if(((p>>8)&255u)==0xbfu){
  uint32_t cc=(p>>16)&127u,value=(p>>24)&127u;
  if(cc==119u){if(value){ht_capture_enabled=1;ht_capture_lease=now;}else ht_capture_stop();}
  if(cc==118u&&value==1u)ht_diagnostics_pending=1;
 }
 if(!usb.config||usb.ota_req||usb.uboot_req||sx_ready||sx_busy){ht_capture_stop();return;}
 if(ht_capture_enabled&&now-ht_capture_lease>3000u)ht_capture_stop();
 if(ht_capture_sending)return;
 if(ht_diagnostics_pending&&now-last_diagnostic>=100u){
  uint32_t values[]={now,ht_render_frames,ht_render_total_us,ht_render_max_us,ht_render_last_us,plat_cpu_pct(),plat_xruns(),om_audio_stat(7),(uint32_t)ht_feed.state};
  uint8_t bytes[51]={0xf0,0x7d,0x44,0x43,1};uint32_t n=5;
  for(uint32_t i=0;i<9;i++)for(uint32_t k=0;k<5;k++)bytes[n++]=(uint8_t)((values[i]>>(k*7u))&127u);bytes[n++]=0xf7;
  uint32_t flags=irq_save();
  if(mo_w-mo_r+17u<=MQ){
   for(uint32_t i=0;i<n;){uint32_t len=n-i>3?3:n-i,cin=i+3<n?4:len+4;uint32_t packet=cin|((uint32_t)bytes[i]<<8);if(len>1)packet|=(uint32_t)bytes[i+1]<<16;if(len>2)packet|=(uint32_t)bytes[i+2]<<24;midi_out_q[mo_w%MQ]=packet;RING_PUBLISH();mo_w++;i+=len;}
   ht_diagnostics_pending=0;last_diagnostic=now;
  }irq_restore(flags);
 }
 if(!ht_capture_enabled||ht_capture_frame==ht_render_frames)return;
 uint8_t raw[HT_SNAPSHOT_BYTES];ht_snapshot_encode(raw,now,plat_xruns(),om_audio_stat(7));
 uint32_t h=ht_snapshot_hash(raw,sizeof raw),n=0;
 ht_capture_message[n++]=0xf0;ht_capture_message[n++]=0x7d;ht_capture_message[n++]=0x48;ht_capture_message[n++]=0x54;ht_capture_message[n++]=1;
 ht_capture_message[n++]=HT_SNAPSHOT_REVISION&127u;ht_capture_message[n++]=(HT_SNAPSHOT_REVISION>>7)&127u;
 ht_capture_message[n++]=ht_render_frames&127u;ht_capture_message[n++]=(ht_render_frames>>7)&127u;
 for(unsigned i=0;i<5;i++)ht_capture_message[n++]=(h>>(7*i))&127u;
 for(unsigned i=0;i<sizeof raw;){unsigned count=sizeof raw-i>7?7:sizeof raw-i,bits=0;for(unsigned j=0;j<count;j++)bits|=((raw[i+j]>>7)&1u)<<j;ht_capture_message[n++]=(uint8_t)bits;for(unsigned j=0;j<count;j++)ht_capture_message[n++]=raw[i+j]&127u;i+=count;}
 ht_capture_message[n++]=0xf7;ht_capture_length=n;ht_capture_offset=0;ht_capture_frame=ht_render_frames;RING_PUBLISH();if(ht_capture_enabled)ht_capture_sending=1;
}
static void capture_tx_poll(void){
 if(!ht_capture_enabled||!ht_capture_sending)return;
 if(!usb.config||usb.ota_req||usb.uboot_req||sx_ready||sx_busy){ht_capture_stop();return;}
 for(unsigned budget=0;budget<16&&ht_capture_offset<ht_capture_length&&mo_w-mo_r<MQ;budget++){
  uint32_t i=ht_capture_offset,left=ht_capture_length-i,k=left>3?3:left,cin=left>3?4:k+4;
  midi_out_q[mo_w%MQ]=cin|((uint32_t)ht_capture_message[i]<<8)|(k>1?(uint32_t)ht_capture_message[i+1]<<16:0)|(k>2?(uint32_t)ht_capture_message[i+2]<<24:0);RING_PUBLISH();mo_w++;ht_capture_offset+=k;
 }
 if(ht_capture_offset==ht_capture_length)ht_capture_sending=0;
}

// SPDX-License-Identifier: BSD-3-Clause

#include "jack_mac_audio.h"
static uint8_t *memory;
static size_t memory_size;
static uint8_t pa,pb;
static unsigned line,rd,wr,count;
static uint32_t source_clock,output_clock;
static int32_t dc;
static int16_t ring[4096];
static void (*vblank)(void);
uint32_t jack_mac_audio_generated,jack_mac_audio_nonzero,jack_mac_audio_overruns;
void jack_mac_audio_init(uint8_t *ram,size_t size){
    memory=ram;memory_size=size;pa=0x4b;pb=0x80;
    line=rd=wr=count=source_clock=output_clock=0;dc=0;vblank=0;
    jack_mac_audio_generated=jack_mac_audio_nonzero=jack_mac_audio_overruns=0;
}
void jack_mac_audio_porta(uint8_t value){pa=value;}
void jack_mac_audio_portb(uint8_t value){pb=value;}
void jack_mac_audio_set_vblank(void (*callback)(void)){vblank=callback;}
size_t jack_mac_screen_offset(void){return memory_size-((pa&0x40)?0x5900:0xd900);}
unsigned jack_mac_audio_level(void){return count;}
int jack_mac_audio_pop(int16_t *s){
    if(!count)return 0;
    *s=ring[rd];rd=(rd+1)&4095;count--;return 1;
}
void jack_mac_audio_advance(unsigned us){
    // The caller slices time so sound-page reads follow CPU writes.

    source_clock+=us*244800u;
    while(source_clock>=11000000u){
        source_clock-=11000000u;
        size_t offset=(pa&8)?0x300:0x5f00;
        int32_t value=0;
        if(memory&&memory_size>=offset+740 && !(pb&0x80)){

            static const unsigned gain[8]={2720,5440,11241,13961,21521,24241,30042,32768};

            value=((int)memory[memory_size-offset+line*2]+97)*64;
            value=(value*(int32_t)gain[pa&7])/32768;
        }

        dc+=(value*256-dc)/256;
        int32_t filtered=value-dc/256;
        if(filtered>32767)filtered=32767;if(filtered<-32768)filtered=-32768;
        line=(line+1)%370;
        if(!line&&vblank)vblank();
        output_clock+=44100u*11u;
        while(output_clock>=244800u){
            output_clock-=244800u;jack_mac_audio_generated++;
            if(filtered)jack_mac_audio_nonzero++;
            if(count==4096){jack_mac_audio_overruns++;continue;}
            ring[wr]=(int16_t)filtered;wr=(wr+1)&4095;count++;
        }
    }
}

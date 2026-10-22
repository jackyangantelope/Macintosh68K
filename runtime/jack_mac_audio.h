// SPDX-License-Identifier: BSD-3-Clause

#ifndef JACK_MAC_AUDIO_H
#define JACK_MAC_AUDIO_H
#include <stddef.h>
#include <stdint.h>
void jack_mac_audio_init(uint8_t *ram,size_t size);
void jack_mac_audio_porta(uint8_t value);
void jack_mac_audio_portb(uint8_t value);

void jack_mac_audio_advance(unsigned us);
void jack_mac_audio_set_vblank(void (*callback)(void));
size_t jack_mac_screen_offset(void);
int jack_mac_audio_pop(int16_t *sample);
unsigned jack_mac_audio_level(void);
extern uint32_t jack_mac_audio_generated,jack_mac_audio_nonzero,jack_mac_audio_overruns;
#endif

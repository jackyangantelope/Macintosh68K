// SPDX-License-Identifier: BSD-3-Clause
#ifndef MAC_NESPAD_INPUT_H
#define MAC_NESPAD_INPUT_H
#include <stdbool.h>
#include "jack_input_devices.h"
#ifdef __cplusplus
extern "C" {
#endif
bool jack_nespad_input_init(jack_input_devices *devices,uint32_t cpu_khz);
bool jack_nespad_input_poll(void);
#ifdef __cplusplus
}
#endif
#endif

// SPDX-License-Identifier: BSD-3-Clause

#ifndef JACK_MAC_SCSI_H
#define JACK_MAC_SCSI_H
#include <stdint.h>
static uint8_t mac_scsi_output,mac_scsi_icr,mac_scsi_mode,mac_scsi_tcr;
static uint8_t jack_mac_scsi_read(unsigned address) {
    switch((address>>4)&7){
    case 0:case 6:return (mac_scsi_icr&1)?mac_scsi_output:0;
    case 1:return mac_scsi_icr;
    case 2:return mac_scsi_mode;
    case 3:return mac_scsi_tcr;
    case 4:return (mac_scsi_icr&0x80)|((mac_scsi_icr&8)?0x40:0)|((mac_scsi_icr&4)?2:0);
    case 5:return (mac_scsi_tcr&7)?0:8;
    default:return 0;
    }
}
static void jack_mac_scsi_write(unsigned address,uint8_t value) {
    switch((address>>4)&7){
    case 0:mac_scsi_output=value;break;
    case 1:mac_scsi_icr=value&0x9f;break;
    case 2:mac_scsi_mode=value;break;
    case 3:mac_scsi_tcr=value&15;break;
    default:break;
    }
}
#endif

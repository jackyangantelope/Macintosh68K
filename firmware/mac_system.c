// SPDX-License-Identifier: BSD-3-Clause

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hardware/clocks.h"
#include "hardware/psram.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "ff.h"
#include "tf_card.h"
#include "hstx.h"
#include "font_8x8.h"
#include "jack_input.h"
#include "jack_input_devices.h"
#include "jack_memory.h"
#include "jack_nespad_input.h"
#include "jack_tinyusb_input.h"
#include "umac.h"
#include "rom.h"
#include "m68k.h"
#include "jack_mac_audio.h"

#define ROM_PATH "/roms/MAC68K/MacPlusV3.rom"
static const char *disk_path="/roms/MAC68K/System608-Missile.dsk";
static const char *save_path="/SAVES/MAC68K/System608-Missile.dsk";
#define DISK_PATH disk_path
#define SAVE_PATH save_path
static unsigned disk_bytes=1474560u;
#define DISK_BYTES disk_bytes
static FATFS filesystem;
static FIL save_file;
static uint8_t mac_rom[ROM_SIZE];
static uint8_t hot_memory[256];
static uint8_t *disk_data;
static uint32_t disk_reads, disk_writes;
static bool disk_dirty, disk_error;
static uint64_t audio_deadline;
static uint32_t audio_under;
static void audio_service(void) {
    uint64_t now=time_us_64()*44100u;
    if(now>audio_deadline+1000000ull*2048)audio_deadline=now;
    while(audio_deadline<=now) {
        int16_t sample=0;if(!jack_mac_audio_pop(&sample))audio_under++;
        hstx_push_audio_sample(sample,sample);audio_deadline+=1000000;
    }
}

static void text_line(const char *msg, unsigned y) {
    uint16_t *p=(uint16_t *)hstx_getframebuffer();
    for(unsigned x=8;*msg && x+8<=320;x+=8,++msg) {
        unsigned c=(unsigned char)*msg; if(c<32 || c>126) continue;
        for(unsigned r=0;r<8;++r) for(unsigned b=0;b<8;++b)
            if((uint8_t)font_8x8[r*95+c-32] & (1u<<b)) p[(y+r)*320+x+b]=0x7fff;
    }
}
static void fail(const char *reason) {
    printf("MAC_ERROR %s\n",reason);
    memset(hstx_getframebuffer(),0,320*240*2);
    text_line("MACINTOSH STARTUP ERROR",72); text_line(reason,96);
    text_line("SEE UART FOR DETAILS",120);
    for(;;) tight_loop_contents();
}
static bool load_exact(const char *path, uint8_t *data, UINT size) {
    FIL f; UINT n=0; if(f_open(&f,path,FA_READ)!=FR_OK) return false;
    bool ok=f_size(&f)==size && f_read(&f,data,size,&n)==FR_OK && n==size;
    f_close(&f); return ok;
}
static bool check_rom(void) {
    if(memcmp(mac_rom,"\x4d\x1f\x81\x72",4)) return false;
    uint32_t sum=0;
    for(unsigned i=4;i<ROM_SIZE;i+=2) sum+=((uint16_t)mac_rom[i]<<8)|mac_rom[i+1];
    return sum==0x4d1f8172u;
}
static bool open_disk(void) {
    // Keep the seed disk untouched; persist changes in a separate working copy.
    FILINFO info;
    FRESULT r=f_stat(SAVE_PATH,&info);
    if(r==FR_NO_FILE || r==FR_NO_PATH) {
        if(!load_exact(DISK_PATH,disk_data,DISK_BYTES)) return false;
        if(memcmp(disk_data,"LK",2) || memcmp(disk_data+1024,"BD",2)) return false;
        r=f_mkdir("/SAVES"); if(r!=FR_OK && r!=FR_EXIST) return false;
        r=f_mkdir("/SAVES/MAC68K"); if(r!=FR_OK && r!=FR_EXIST) return false;

        if(f_open(&save_file,SAVE_PATH,FA_READ|FA_WRITE|FA_CREATE_NEW)!=FR_OK) return false;
        UINT n=0;
        if(f_write(&save_file,disk_data,DISK_BYTES,&n)!=FR_OK || n!=DISK_BYTES ||
           f_sync(&save_file)!=FR_OK) { f_close(&save_file); return false; }
    } else {
        if(r!=FR_OK || info.fsize!=DISK_BYTES || !load_exact(SAVE_PATH,disk_data,DISK_BYTES)) return false;
        if(f_open(&save_file,SAVE_PATH,FA_READ|FA_WRITE)!=FR_OK) return false;
    }
    return memcmp(disk_data,"LK",2)==0 && memcmp(disk_data+1024,"BD",2)==0;
}
static int disk_read(void *ctx,uint8_t *dst,unsigned pos,unsigned len) {
    (void)ctx;
    if(pos>DISK_BYTES || len>DISK_BYTES-pos) return -1;
    memcpy(dst,disk_data+pos,len); ++disk_reads; return 0;
}
static int disk_write(void *ctx,uint8_t *src,unsigned pos,unsigned len) {
    (void)ctx; UINT n=0;
    if(pos>DISK_BYTES || len>DISK_BYTES-pos) return -1;
    if(f_lseek(&save_file,pos)!=FR_OK || f_write(&save_file,src,len,&n)!=FR_OK || n!=len) {
        disk_error=true; return -1;
    }
    memcpy(disk_data+pos,src,len); disk_dirty=true; ++disk_writes; return 0;
}
static void render(void) {
    const uint8_t *src=ram_get_base()+jack_mac_screen_offset();
    uint16_t *dst=(uint16_t *)hstx_getframebuffer();

    for(unsigned y=0;y<214;++y) {
        unsigned sy=y*342/214;
        const uint8_t *row=src+sy*64;
        uint16_t *out=dst+(y+13)*320;

        for(unsigned b=0;b<64;++b) {
            uint8_t bits=row[b];
            out[0]=(bits&0x80u)?0:0x7fff;
            out[1]=(bits&0x40u)?0:0x7fff;
            out[2]=(bits&0x10u)?0:0x7fff;
            out[3]=(bits&0x08u)?0:0x7fff;
            out[4]=(bits&0x02u)?0:0x7fff;
            out+=5;
        }
    }
}

static int mac_key(uint8_t u) {
    static const uint8_t letters[]={0,11,8,2,14,3,5,4,34,38,40,37,46,45,31,35,12,15,1,17,32,9,13,7,16,6};
    static const uint8_t digits[]={18,19,20,21,23,22,26,28,25,29};
    if(u>=4 && u<=29) return letters[u-4];
    if(u>=30 && u<=39) return digits[u-30];
    switch(u) {
    case 40:return 36; case 41:return 53; case 42:return 51; case 43:return 48;
    case 44:return 49; case 45:return 27; case 46:return 24; case 47:return 33;
    case 48:return 30; case 49:return 42; case 51:return 41; case 52:return 39;
    case 53:return 50; case 54:return 43; case 55:return 47; case 56:return 44;
    default:return -1;
    }
}
static void key_event(unsigned key,bool down) { umac_kbd_event((uint8_t)((key<<1)|1),down); }
static void keyboard(const jack_input_devices *d) {
    static uint8_t old[6],oldmods;
    const uint8_t mods=d->keyboard_modifier;
    const uint8_t masks[]={0x88,0x22,0x44,0x11},keys[]={55,56,58,59};
    for(unsigned k=0;k<4;++k) if(!!(oldmods&masks[k])!=!!(mods&masks[k])) key_event(keys[k],(mods&masks[k])!=0);
    for(unsigned i=0;i<6;++i) {
        bool held=false;
        for(unsigned j=0;j<6;++j) if(old[i]==d->keyboard_keys[j]) held=true;
        int k=mac_key(old[i]); if(!held && k>=0) key_event((unsigned)k,false);
    }
    for(unsigned i=0;i<6;++i) {
        bool held=false;
        for(unsigned j=0;j<6;++j) if(d->keyboard_keys[i]==old[j]) held=true;
        int k=mac_key(d->keyboard_keys[i]); if(!held && k>=0) key_event((unsigned)k,true);
    }
    memcpy(old,d->keyboard_keys,6);oldmods=mods;
}
static void inputs(jack_input_devices *d,jack_input_queue *q,bool usb,bool nes,bool tick) {
    static int32_t oldx,oldy;
    static uint16_t oldpad;
    static bool game_keys;
    static uint64_t oldkeys;
    if(usb) jack_tinyusb_input_poll();
    if(nes) (void)jack_nespad_input_poll();
    keyboard(d);
    jack_input_event event; while(jack_input_pop(q,&event)) {  }
    uint16_t pad=d->pads[0];
    const uint16_t combo=JACK_PAD_SELECT|JACK_PAD_START;
    if((pad&combo)==combo && (oldpad&combo)!=combo) {
        game_keys=!game_keys;
        printf("MAC_PAD mode=%s\n",game_keys?"GLIDER_KEYS":"DESKTOP_MOUSE");
    }
    uint16_t controls=(pad&combo)==combo?(pad&~combo):pad;
    int dx=(int)(d->mouse_x-oldx),dy=(int)(d->mouse_y-oldy);oldx=d->mouse_x;oldy=d->mouse_y;
    if(tick&&!game_keys) {
        int step=(pad&JACK_PAD_B)?1:2;
        dx+=((pad&JACK_PAD_RIGHT)?step:0)-((pad&JACK_PAD_LEFT)?step:0);
        dy+=((pad&JACK_PAD_DOWN)?step:0)-((pad&JACK_PAD_UP)?step:0);
    }
    umac_mouse(dx,-dy,(d->mouse_buttons&1) || (!game_keys&&(controls&JACK_PAD_A)));
    uint64_t keys=0;
    if(controls&JACK_PAD_START)keys|=1ull<<36;
    if(controls&JACK_PAD_SELECT)keys|=1ull<<55;
    if(controls&JACK_PAD_X)keys|=1ull<<45;
    if(controls&JACK_PAD_Y)keys|=1ull<<31;
    if(game_keys){
        if(controls&JACK_PAD_LEFT)keys|=1ull<<43;
        if(controls&JACK_PAD_RIGHT)keys|=1ull<<47;
        if(controls&JACK_PAD_A)keys|=1ull<<49;
        if(controls&JACK_PAD_B)keys|=1ull<<36;
    }
    uint64_t changed=keys^oldkeys;
    if(changed)for(unsigned k=0;k<64;k++)if(changed&(1ull<<k))key_event(k,(keys&(1ull<<k))!=0);
    oldkeys=keys;
    oldpad=pad;
}
int main(void) {
    stdio_init_all();vreg_set_voltage(VREG_VOLTAGE_1_30);sleep_ms(10);
    if(!set_sys_clock_khz(252000,true)) return 2;
    stdio_init_all();sleep_ms(200);printf("MAC_BOOT app=%08x\n",(unsigned)JACK_APP_BASE);
    bool present=psram_is_available();
    int params=present?psram_configure_params(PICO_DEFAULT_PSRAM_MAX_FREQ,
        PICO_DEFAULT_PSRAM_MAX_SELECT,PICO_DEFAULT_PSRAM_MIN_DESELECT):PICO_ERROR_NOT_FOUND;
    uint32_t irq=save_and_disable_interrupts();
    int reinit=params==PICO_OK?psram_reinitialize():params;restore_interrupts(irq);
    jack_memory_layout memory;
    bool memory_ok=present && reinit==PICO_OK && jack_memory_init(&memory,hot_memory,
        sizeof(hot_memory),(void *)0x11000000u,psram_get_size());
    pico_fatfs_spi_config_t config={SDCARD_SPI,CLK_SLOW_DEFAULT,CLK_FAST_DEFAULT_PIO,
        SDCARD_PIN_MISO,SDCARD_PIN_CS,SDCARD_PIN_SCK,SDCARD_PIN_MOSI,true};
    bool cfg=pico_fatfs_set_config(&config);
    FRESULT sd=cfg?f_mount(&filesystem,"",1):FR_NOT_READY;
    hstx_init(false);
    memset(hstx_getframebuffer(),0,320*240*2);
    text_line("MAC PLUS 0.3.0 / SAMPLED AUDIO",80);
    text_line("LOADING ROM AND STARTUP DISK",104);
    printf("MAC_BOOT psram=%u size=%lu sd=%d\n",memory_ok,(unsigned long)psram_get_size(),sd);
    if(!memory_ok) fail("PSRAM INITIALIZATION FAILED");
    if(sd!=FR_OK) fail("SD MOUNT FAILED");
    FILINFO game_info;
    if(f_stat("/roms/MAC68K/System608-Glider.dsk",&game_info)==FR_OK) {
        if(game_info.fsize!=4u*1024u*1024u)fail("INVALID GLIDER BOOT DISK SIZE");
        disk_path="/roms/MAC68K/System608-Glider.dsk";
        save_path="/SAVES/MAC68K/System608-Glider.dsk";
        disk_bytes=4u*1024u*1024u;
    }
    printf("MAC_DISK %s\n",disk_path);
    uint8_t *ram=jack_memory_alloc_bulk(&memory,RAM_SIZE,4);
    disk_data=jack_memory_alloc_bulk(&memory,DISK_BYTES,4);
    if(!ram || !disk_data) fail("PSRAM ALLOCATION FAILED");
    if(!load_exact(ROM_PATH,mac_rom,ROM_SIZE) || !check_rom()) fail("NEED MAC PLUS V3 ROM");
    if(!open_disk()) fail("STARTUP / SAVE DISK ERROR");
    if(rom_patch(mac_rom)) fail("ROM PATCH FAILED");
    memset(ram,0,RAM_SIZE);
    disc_descr_t discs[DISC_NUM_DRIVES]={{.size=DISK_BYTES,.read_only=0,
        .op_read=disk_read,.op_write=disk_write},{0}};
    jack_mac_audio_init(ram,RAM_SIZE);umac_init(ram,mac_rom,discs);
    jack_mac_audio_set_vblank(umac_vsync_event);
    jack_input_queue queue; jack_input_devices devices;jack_input_init(&queue);
    jack_input_devices_init(&devices,&queue);
    bool usb=jack_tinyusb_input_init(&devices),nes=false;
    memset(hstx_getframebuffer(),0,320*240*2);
    printf("MAC_READY ram=%u psram_used=%lu usb=%u nes=%u\n",RAM_SIZE,(unsigned long)memory.bulk.used,usb,nes);
    uint64_t next_cpu=time_us_64(),next_video=next_cpu,next_second=next_cpu+1000000;
    unsigned frames=0,loops=0;
    audio_deadline=time_us_64()*44100u;
    for(;;) {
        uint64_t now=time_us_64();bool video=now>=next_video;
        inputs(&devices,&queue,usb,nes,video);
        audio_service();
        if(now>=next_cpu) {
            if(umac_loop()) fail("CPU STOPPED - CHECK UART");
            audio_service();
            ++loops;next_cpu+=5000;
            if(now>next_cpu+100000) next_cpu=now;
        }
        if(video) {
            render();++frames;
            do { next_video+=16667; } while(next_video<=now);
        }
        if(now>=next_second) {
            umac_1hz_event();next_second=now+1000000;
            if(disk_dirty) {if(f_sync(&save_file)!=FR_OK) disk_error=true;disk_dirty=false;}
            if(disk_error) fail("SAVE DISK WRITE FAILED");
            printf("MAC_ALIVE frames=%u loops=%u pc=%08x read=%lu write=%lu ticks=%lu mx=%ld my=%ld mb=%u pad=%04x\n",
                frames,loops,m68k_get_reg(NULL,M68K_REG_PC),(unsigned long)disk_reads,
                (unsigned long)disk_writes,(unsigned long)RAM_RD32(0x16a),
                (long)devices.mouse_x,(long)devices.mouse_y,devices.mouse_buttons,devices.pads[0]);
            printf("MAC_AUDIO generated=%lu active=%lu queue=%u under=%lu over=%lu\n",
                (unsigned long)jack_mac_audio_generated,(unsigned long)jack_mac_audio_nonzero,
                jack_mac_audio_level(),(unsigned long)audio_under,(unsigned long)jack_mac_audio_overruns);
        }
        tight_loop_contents();
    }
}

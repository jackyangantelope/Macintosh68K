"""Apply bounded integration fixes to build-local copies of the pinned umac.

Upstream files and their license notices remain intact. Fail if the pinned
source no longer matches.
"""
import argparse
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--umac", type=Path, required=True)
p.add_argument("--out", type=Path, required=True)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Pinned source mismatch: {old[:65]!r}")
    return text.replace(old, new, 1)

main = (a.umac / "src/main.c").read_text()
main = '#include "jack_mac_audio.h"\n#include "jack_mac_scsi.h"\n' + main
# Mac Plus A17 disables the ROM chips in the 0x420000 window.
for macro, inactive in (("ROM_RD8","0xffu"),("ROM_RD16","0xffffu"),
                        ("ROM_RD32","0xffffffffu"),("ROM_RD_ALIGNED_BE16","0xffffu")):
    old=f"return {macro}(address & (ROM_SIZE - 1));"
    if not main.count(old):raise ValueError(f"Pinned ROM access missing: {macro}")
    main=main.replace(old,f"return (address & 0x20000u) ? {inactive} : {macro}(address & (ROM_SIZE - 1));")
main = replace_once(main,"        // decode IO etc",
    """        if ((address & 0xf80000u)==0x580000u) return jack_mac_scsi_read(address);
        // decode IO etc""")
main = replace_once(main,"        // decode IO\n",
    """        if ((address & 0xf80000u)==0x580000u) { jack_mac_scsi_write(address,value); return; }
        // decode IO
""")
main = replace_once(main, """static uint8_t  via_ra_in(void)
{
        return 0;
}""", """static uint8_t  via_ra_in(void)
{
        /* Mac Plus PA7/PA0 input levels, as wired on the original machine. */
        return 0x81;
}""")
main = replace_once(main,"        // [2:0] = sound volume",
    "        jack_mac_audio_porta(val);\n        // [2:0] = sound volume")
main = replace_once(main,"        // 7 = sndres (sound enable/disable)",
    "        jack_mac_audio_portb(val);\n        // 7 = sndres (sound enable/disable)")
main = replace_once(main,"        m68k_execute(us*8);",
    """        /* Preserve the existing total instruction budget, but interleave
         * sound-page reads with CPU writes at approximately one scanline. */
        for (unsigned elapsed=0; elapsed<(unsigned)us;) {
                unsigned slice=(unsigned)us-elapsed;
                if(slice>45) slice=45;
                m68k_execute(slice*8);
                jack_mac_audio_advance(slice);
                elapsed+=slice;
        }""")
main = replace_once(main, "static int kbd_pending_evt = -1;",
    "static uint8_t kbd_events[64];\nstatic unsigned kbd_head, kbd_tail;")
main = replace_once(main, "if (kbd_pending_evt == -1)", "if (kbd_head == kbd_tail)")
main = replace_once(main, "via_sr_rx(kbd_pending_evt);\n                        kbd_pending_evt = -1;",
    "#ifdef MAC_HOST_TRACE_INPUT\n                        printf(\"MAC_KEY_RX %02x\\n\",kbd_events[kbd_tail]);\n#endif\n"
    "                        via_sr_rx(kbd_events[kbd_tail]);\n                        kbd_tail = (kbd_tail + 1u) % 64u;")
start = main.index("void    umac_kbd_event(")
end = main.index("// VIA IRQ output hook:", start)
main = main[:start] + '''void umac_kbd_event(uint8_t scancode, int down)
{
    unsigned next = (kbd_head + 1u) % 64u;
    if (next == kbd_tail) {
        fprintf(stderr, "MAC keyboard queue full\\n");
        return;
    }
    kbd_events[kbd_head] = scancode | (down ? 0 : 0x80);
#ifdef MAC_HOST_TRACE_INPUT
    printf("MAC_KEY_TX %02x\\n",kbd_events[kbd_head]);
#endif
    kbd_head = next;
}

''' + main[end:]
main = replace_once(main, "        setjmp(main_loop_jb);", "        if (setjmp(main_loop_jb)) return 1;")
main = replace_once(main, "        overlay = 1;\n        m68k_pulse_reset();",
    "        overlay = 1;\n        update_overlay_layout();\n        m68k_pulse_reset();")
(a.out / "main.c").write_text(main)

disc = (a.umac / "src/disc.c").read_text()
disc = replace_once(disc, "        if ((position + length) > info->size)",
    "        if (position > info->size || length > info->size - position)")
disc = replace_once(disc, "\tsize_t actual = 0;", "\tsize_t actual = length;")
(a.out / "disc.c").write_text(disc)

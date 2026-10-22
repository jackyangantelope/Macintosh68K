# Third-party provenance

| Component | Pinned source | License and included notice |
|---|---|---|
| umac | [evansm7/umac](https://github.com/evansm7/umac) `606f7f75fee8525d4735a0771bf521007115285e`, via [pico-mac](https://github.com/evansm7/pico-mac) `c8c4d944f28fc63c70b45595e56b94a00f4d3899` | Mainly MIT (`deps/umac/README.md`, `deps/umac/LICENSE`). Basilisk II-derived `disc.c`, `b2_macos_util.h` and disk-driver material retain GPL-2.0-or-later notices in the source. |
| Musashi | [evansm7/Musashi](https://github.com/evansm7/Musashi) `0b85a7a648e7b4d279b41e096c4d67ad354a82f7` (fork of Karl Stenerud's Musashi) | MIT (`deps/Musashi/readme.txt`, `deps/Musashi/LICENSE`); two files adapted for 68000-only use. SoftFloat 2b is excluded. |
| pico_shared | [fhoedemakers/pico_shared](https://github.com/fhoedemakers/pico_shared) `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0` | GPL-3.0-only (`deps/pico_shared/LICENSE`); FatFs carries its own license (`deps/pico_shared/drivers/pico_fatfs/LICENSE`). |

No Apple ROM, Mac OS image, game disk, save file or SoftFloat source is included. Raspberry Pi Pico SDK, Pico-PIO-USB and TinyUSB are external build dependencies.

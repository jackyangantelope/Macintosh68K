# Macintosh 68K source for RP2350B

This is a source-only Mac Plus-class emulator frontend for the public Adafruit Fruit Jam (RP2350B). It builds a loader application at `0x10080000`, **not** a standalone firmware image. No Apple ROM, system disk, game disk, save data, or prebuilt UF2 is included.

## Status

The project owner reports that the original board build passed basic physical testing. The local 0.3.0 System 6/Glider development work also passed host-side game and sampled-audio checks. Those observations do not validate this public Fruit Jam variant: its HDMI pins, SD pins, USB host pins and input configuration differ, and it has **not** been tested on a Fruit Jam. Long-duration play, audio listening, save readback after restart and power-loss recovery remain unverified for this release.

The emulator provides a 68000 CPU, 2 MiB guest RAM, Mac Plus V3 ROM loading, one writable startup disk, 512×342 monochrome display scaled to 320×214, USB keyboard/mouse, and sampled mono sound sent through HDMI. It is not a general Macintosh compatibility guarantee. SCSI devices, multiple disks, complete VIA timers and serial networking are not implemented.

## Build

Install CMake 3.20+, Ninja, Python 3, ARM embedded GCC, a Raspberry Pi Pico SDK 2.3.x with RP2350/Fruit Jam and PSRAM support, and Pico-PIO-USB. TinyUSB comes from the SDK. Set `PICO_SDK_PATH` and `PICO_PIO_USB_PATH` to local checkout paths:

```sh
cmake -S . -B build/fruit-jam-release -G Ninja \
  -DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=adafruit_fruit_jam \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/fruit-jam-release --target picoMac68kSystem
```

On Windows, `scripts/build-release.ps1` accepts explicit SDK, Pico-PIO-USB, toolchain, Ninja and Python paths. The result is `build/fruit-jam-release/picoMac68kSystem.uf2`. Flash it only through a compatible loader after checking the target hardware.

## User-provided content

Supply your own legally obtained 128 KiB Mac Plus V3 ROM at `/roms/MAC68K/MacPlusV3.rom`. Supply a compatible startup disk at `/roms/MAC68K/System608-Glider.dsk` (4 MiB) or `/roms/MAC68K/System608-Missile.dsk` (1,474,560 bytes). The firmware prefers the Glider filename if it exists. The first run creates a distinct writable disk under `/SAVES/MAC68K/`; preserve backups before replacing media. This repository grants no right to redistribute the ROM, Mac OS or games.

USB keyboard and mouse are enabled. The original board's gamepad adapter is disabled in this Fruit Jam build. A compatible HDMI display may carry audio; this public wiring is not yet physically verified.

## Sources and changes

- [evansm7/pico-mac](https://github.com/evansm7/pico-mac), commit `c8c4d944f28fc63c70b45595e56b94a00f4d3899`, pins [umac](https://github.com/evansm7/umac) commit `606f7f75fee8525d4735a0771bf521007115285e` and its [Musashi fork](https://github.com/evansm7/Musashi) commit `0b85a7a648e7b4d279b41e096c4d67ad354a82f7`. Selected files and original notices are in `deps/`. See [THIRD_PARTY.md](THIRD_PARTY.md) for the mixed MIT/GPL notices.
- `scripts/materialize-mac-runtime.py` makes build-local copies of umac `main.c` and `disc.c` with bounded changes: Plus ROM address decoding, empty SCSI register handling, sound-page timing, keyboard FIFO, ROM reset mapping, and disk transfer bounds/results. Upstream source stays unchanged. Host-only diagnostic file dumps from the development script are omitted.
- `deps/Musashi/m68kcpu.c` and `m68kcpu.h` are adapted for the 68000-only release. FPU and PMMU entry points report the illegal-instruction exception or leave address translation unchanged; the non-standard-licensed SoftFloat 2b implementation is not included or linked. This does not add 68020/030/040 support. The checked-in `m68kops.c` is the pinned generated/decorated opcode table (SHA-256 `145666b747d7cd97ce191debd72e75de200187b7cb7bf75ac308205ba085a166`).
- `runtime/jack_mac_audio.*` adds sampled audio and screen-page selection; `runtime/jack_mac_scsi.h` supplies an empty SCSI bus. `firmware/mac_system.c` adds SD/PSRAM/HDMI/USB integration and a separate writable disk. `platform/` supplies BSD-3-Clause adapters. The release uses Fruit Jam HSTX, SD, PSRAM and USB-host pins with keyboard/mouse input.

The combined program is offered under [GPL-3.0-only](LICENSE), with original component notices retained. The project's own BSD-3-Clause notice is in [LICENSE.BSD-3-Clause](LICENSE.BSD-3-Clause). External SDK/Pico-PIO-USB dependencies are not vendored.
